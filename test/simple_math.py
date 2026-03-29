def add_numbers(a, b):
    total = a + b
    return total * 2


def summarize(values):
    cleaned = [value for value in values if value >= 0]
    return {
        "count": len(cleaned),
        "sum": sum(cleaned),
        "avg": sum(cleaned) / len(cleaned) if cleaned else 0,
    }


if __name__ == "__main__":
    print(add_numbers(2, 5))
    print(summarize([1, -3, 7, 9]))
