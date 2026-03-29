def describe_status(code):
    match code:
        case 200:
            return "ok"
        case 400 | 404:
            return "client_error"
        case value if value >= 500:
            return "server_error"
        case _:
            return "unknown"


def load_payload(payload):
    try:
        return {key: value for key, value in payload.items() if value is not None}
    except AttributeError as exc:
        raise ValueError("payload must be a mapping") from exc


print(describe_status(503))
print(load_payload({"name": "tzshot", "extra": None}))
