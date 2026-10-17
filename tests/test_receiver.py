"""Receiver checks use synthetic responses; they do not measure a person."""
import importlib.util
import json
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / "examples" / "receive_arduino.py"
spec = importlib.util.spec_from_file_location("receiver", path)
receiver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(receiver)


class Response:
    def __init__(self, data):
        self.body = json.dumps(data).encode()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass

    def read(self, size):
        return self.body[:size]


class Opener:
    def __init__(self, data):
        self.data = data

    def open(self, url, timeout):
        return Response(self.data)


class ReceiverTests(unittest.TestCase):
    def test_available_and_unavailable_fields_are_preserved(self):
        sample = {"status": "invalid", "bpm": 75, "spo2": None,
                  "chip_temperature_c": 25.5, "finger_present": True}
        data = receiver.fetch_readings("http://example.invalid/api", 2,
                                       Opener(sample))
        self.assertEqual(data["bpm"], 75)
        self.assertIsNone(data["spo2"])
        self.assertEqual(data["chip_temperature_c"], 25.5)
        self.assertTrue(data["transport_connected"])

    def test_invalid_response_is_rejected(self):
        for sample in ([], {"spo2": 97}, {"status": "ok", "bpm": True},
                       {"status": "ok", "spo2": "97"},
                       {"status": "ok", "spo2": float("nan")}):
            with self.subTest(sample=sample), self.assertRaises(ValueError):
                receiver.fetch_readings("http://example.invalid/api", 2,
                                        Opener(sample))

    def test_disconnection_clears_all_measurements(self):
        data = receiver.disconnected_readings(TimeoutError("timed out"))
        for field in ("bpm", "spo2", "chip_temperature_c", "ir"):
            self.assertIsNone(data[field])
        self.assertFalse(data["finger_present"])
        self.assertFalse(data["transport_connected"])
        self.assertEqual(data["status"], "disconnected")

    def test_poll_interval_must_be_positive_and_finite(self):
        for value in ("0", "-1", "nan", "inf"):
            with self.subTest(value=value), self.assertRaises(
                    receiver.argparse.ArgumentTypeError):
                receiver.positive_seconds(value)


if __name__ == "__main__":
    unittest.main()
