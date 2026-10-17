#!/usr/bin/env python3
"""Read the MAX30102 Arduino HTTP API and print one JSON record per poll.

Join the Arduino access point before running this script. Python 3 standard
library only; no pip installation is needed. This is a polling receiver, not
a POST transmitter to the team's separate /telemetry backend.
"""

import argparse
from datetime import datetime, timezone
import json
import math
import sys
import time
from urllib.request import ProxyHandler, build_opener


def positive_seconds(value):
    seconds = float(value)
    if not math.isfinite(seconds) or seconds <= 0:
        raise argparse.ArgumentTypeError("must be a positive, finite number")
    return seconds


def fetch_readings(url, timeout, opener=None):
    """Return the Arduino JSON dictionary, preserving null/invalid readings."""
    if opener is None:
        # This is a direct local link. Do not send it through an HTTP proxy.
        opener = build_opener(ProxyHandler({}))
    with opener.open(url, timeout=timeout) as response:
        body = response.read(65537)
    if len(body) > 65536:
        raise ValueError("API response exceeds 64 KiB")
    data = json.loads(body)
    if not isinstance(data, dict) or not isinstance(data.get("status"), str):
        raise ValueError("response is not an Arduino API status object")
    for field in ("bpm", "spo2", "chip_temperature_c"):
        value = data.get(field)
        if value is not None and (
            isinstance(value, bool)
            or not isinstance(value, (int, float))
            or not math.isfinite(value)
        ):
            raise ValueError(f"{field} must be a finite number or null")
    data["transport_connected"] = True
    data["received_at_utc"] = datetime.now(timezone.utc).isoformat()
    return data


def disconnected_readings(error):
    """Clear current values so connection loss cannot reuse old measurements."""
    return {
        "bpm": None,
        "spo2": None,
        "chip_temperature_c": None,
        "ir": None,
        "finger_present": False,
        "status": "disconnected",
        "message": str(error),
        "transport_connected": False,
        "received_at_utc": datetime.now(timezone.utc).isoformat(),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://192.168.4.1/api")
    parser.add_argument("--interval", type=positive_seconds, default=1.0,
                        help="seconds between completed requests (default: 1)")
    parser.add_argument("--timeout", type=positive_seconds, default=2.0,
                        help="network timeout in seconds (default: 2)")
    parser.add_argument("--once", action="store_true", help="read once and exit")
    parser.add_argument("--pretty", action="store_true", help="indent JSON output")
    args = parser.parse_args(argv)
    opener = build_opener(ProxyHandler({}))
    try:
        while True:
            try:
                data = fetch_readings(args.url, args.timeout, opener)
                failed = False
            except (OSError, ValueError) as error:
                data = disconnected_readings(error)
                failed = True
                print(f"Arduino unavailable: {error}", file=sys.stderr)
            # Integrate your dashboard/HUD update here; keep None as unavailable.
            print(json.dumps(data, indent=2 if args.pretty else None,
                             allow_nan=False), flush=True)
            if args.once:
                return 1 if failed else 0
            time.sleep(args.interval)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
