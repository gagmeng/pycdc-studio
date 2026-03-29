def make_multiplier(base):
    history = []

    def inner(value):
        result = base * value
        history.append(result)
        return result, list(history)

    return inner


triple = make_multiplier(3)
print(triple(4))
print(triple(6))

pairs = [(index, triple(index)[0]) for index in range(1, 4)]
print(pairs)
