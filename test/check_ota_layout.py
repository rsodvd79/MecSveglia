import csv
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
board = json.loads((root / "board/esp32-s3-fh4r2.json").read_text())
flash_size = int(board["upload"]["flash_size"].removesuffix("MB")) * 1024 * 1024
with (root / "partitions.csv").open() as source:
    rows = [
        [field.strip() for field in row]
        for row in csv.reader(line for line in source if not line.lstrip().startswith("#"))
        if row
    ]

partitions = [
    dict(name=row[0], type=row[1], subtype=row[2],
         offset=int(row[3], 0), size=int(row[4], 0))
    for row in rows
]
assert len({p["name"] for p in partitions}) == len(partitions), "Duplicate partition"
end = 0x9000
for p in sorted(partitions, key=lambda p: p["offset"]):
    alignment = 0x10000 if p["type"] == "app" else 0x1000
    assert p["offset"] % alignment == 0, f"Unaligned partition: {p['name']}"
    assert p["size"] > 0 and p["size"] % 0x1000 == 0
    assert p["offset"] >= end, f"Overlapping partition: {p['name']}"
    end = p["offset"] + p["size"]
    assert end <= flash_size, f"Partition exceeds flash: {p['name']}"

slots = [p for p in partitions if p["type"] == "app"]
assert {p["subtype"] for p in slots} == {"ota_0", "ota_1"} and len(slots) == 2
assert slots[0]["size"] == slots[1]["size"], "OTA slots must have equal capacity"
assert any(p["type"] == "data" and p["subtype"] == "ota" and p["size"] >= 0x2000
           for p in partitions), "Missing OTA selection data"
filesystem = next(p for p in partitions if p["name"] == "spiffs")
build = root / ".pio/build/esp32-s3-fh4r2"
firmware_size = (build / "firmware.bin").stat().st_size
filesystem_size = (build / "spiffs.bin").stat().st_size
for slot in slots:
    assert firmware_size <= slot["size"], f"Firmware exceeds {slot['name']}"
assert filesystem_size == filesystem["size"], "Filesystem image does not match layout"
print(f"OTA layout OK: firmware {firmware_size} B / slot {slots[0]['size']} B; "
      f"filesystem {filesystem_size} B; flash {flash_size} B")
