class InventoryItem:
    tax_rate = 0.13

    def __init__(self, name, price, quantity=1):
        self.name = name
        self.price = price
        self.quantity = quantity

    def subtotal(self):
        return self.price * self.quantity

    def total_with_tax(self):
        return round(self.subtotal() * (1 + self.tax_rate), 2)


class InventoryService:
    def __init__(self):
        self._items = []

    def add_item(self, item):
        self._items.append(item)

    def totals(self):
        return [item.total_with_tax() for item in self._items]


service = InventoryService()
service.add_item(InventoryItem("Keyboard", 299, 2))
service.add_item(InventoryItem("Mouse", 99, 1))
print(service.totals())
