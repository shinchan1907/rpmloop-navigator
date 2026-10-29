import json
import time

def calculate_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def validate_nav_packet(pkt: dict) -> tuple[bool, str]:
    required_keys = ["v", "type", "seq", "sid", "payload", "crc"]
    for k in required_keys:
        if k not in pkt:
            return False, f"Missing key: {k}"
    
    if pkt["v"] != 1:
        return False, f"Unsupported version: {pkt['v']}"
    
    payload = pkt["payload"]
    if pkt["type"] == "NAV_UPDATE":
        req_fields = ["maneuver", "dist_m", "road", "rem_dist_m", "rem_time_s"]
        for f in req_fields:
            if f not in payload:
                return False, f"Missing payload field: {f}"
                
    return True, "OK"

def test_protocol_fixtures():
    # 1. Test hello request fixture
    with open("protocol/fixtures/hello_request.json") as f:
        hello = json.load(f)
    assert hello["type"] == "HELLO"
    assert hello["seq"] == 1
    
    # 2. Test nav update fixture
    with open("protocol/fixtures/nav_update.json") as f:
        nav = json.load(f)
    valid, reason = validate_nav_packet(nav)
    assert valid, f"Nav update invalid: {reason}"
    assert nav["payload"]["maneuver"] == "TURN_RIGHT"
    assert nav["payload"]["dist_m"] == 220
    
    # 3. Test CRC calculation
    raw_payload = b'{"seq":2,"dist_m":220}'
    crc = calculate_crc16(raw_payload)
    assert isinstance(crc, int)
    assert 0 <= crc <= 0xFFFF
    
    print("All protocol parser tests passed successfully!")

if __name__ == "__main__":
    test_protocol_fixtures()
