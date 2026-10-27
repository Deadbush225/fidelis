/*
 * Inventory and order desk
 * A small terminal workflow for signing in staff, adjusting stock, and
 * checking out an order. The sample intentionally exercises nested blocks,
 * literals, operators, comments, and malformed input for lexer testing.
 */

// Shared inventory data
int employeePin = 4721;
int orderNumber = 8042;
[string] productNames = ["Coffee", "Tea", "Cocoa"];
[int] stockLevels = [18, 7, 0];
[float] unitPrices = [12.50, 8.25, 15.00];
float taxRate = 0.0825;
float discountRate = 0.10;
bool storeOpen = true;
char shelfZone = 'B';
char lineBreak = '\n';
string registerName = "North \"Market\" register";

react (stockLevels) {
    print("[STOCK] Inventory changed; refresh the product display.");
}

fn calculateTotal(float unitPrice, int quantity) -> float {
    float subtotal = unitPrice * quantity;
    float discount = subtotal * discountRate;
    float taxedTotal = (subtotal - discount) * (1.0 + taxRate);
    return taxedTotal;
}

fn normalizeName(string value) -> string {
    return replace(value, " ", "");
}

fn stockLabel(int quantity) -> string {
    if (quantity == 0) {
        return "OUT OF STOCK";
    } else if (quantity <= 3) {
        return "LOW STOCK";
    } else {
        return "AVAILABLE";
    }
}

state SignIn {
    print("Employee PIN:");
    int? enteredPin = readInt();

    int pin = ensure enteredPin else {
        print("PIN must contain digits.");
        transition to SignIn;
    }

    if (pin == employeePin && storeOpen) {
        print("Welcome to " + registerName + ".");
        transition to Inventory;
    } else {
        print("Sign-in failed or the store is closed.");
        transition to SignIn;
    }
}

state Inventory {
    print("\n--- INVENTORY ---");
    for (int i = 0; i < length(productNames); i += 1) {
        string label = stockLabel(stockLevels[i]);
        print(productNames[i] + ": " + label + " (" + stockLevels[i] + ")");
    }

    loop {
        print("\n1: Adjust stock  2: New order  3: Close register");
        string action = readLine();

        match action {
            "1" -> transition to StockAdjustment;
            "2" -> transition to Checkout;
            "3" -> transition to SignIn;
            ""  -> print("Please choose an action.");
            _   -> print("Unknown action.");
        }
    }
}

state StockAdjustment {
    print("Product number (0-2):");
    int? productInput = readInt();
    int productIndex = ensure productInput else {
        print("Product number must be an integer.");
        transition to StockAdjustment;
    }

    if (productIndex < 0 || productIndex >= length(stockLevels)) {
        print("No product exists at that number.");
        transition to Inventory;
    }

    print("Quantity change (negative values remove stock):");
    int? quantityInput = readInt();
    int quantityChange = ensure quantityInput else {
        print("Quantity must be an integer.");
        transition to StockAdjustment;
    }

    stockLevels[productIndex] += quantityChange;
    if (stockLevels[productIndex] < 0) {
        stockLevels[productIndex] = 0;
    } else if (stockLevels[productIndex] > 999) {
        stockLevels[productIndex] = 999;
    }

    print("Updated " + productNames[productIndex] + ": "
        + stockLevels[productIndex]);
    transition to Inventory;
}

state Checkout {
    print("Product number:");
    int? productInput = readInt();
    int productIndex = ensure productInput else {
        print("Product number must be an integer.");
        transition to Checkout;
    }

    if (productIndex < 0 || productIndex >= length(productNames)) {
        print("Unknown product.");
        transition to Inventory;
    } else if (stockLevels[productIndex] == 0) {
        print(productNames[productIndex] + " is out of stock.");
        transition to Inventory;
    } else {
        print("Quantity:");
        int? quantityInput = readInt();
        int quantity = ensure quantityInput else {
            print("Quantity must be an integer.");
            transition to Checkout;
        }

        if (quantity <= 0 || quantity > stockLevels[productIndex]) {
            print("Quantity is outside the available stock.");
            transition to Inventory;
        } else {
            float total = calculateTotal(unitPrices[productIndex], quantity);
            stockLevels[productIndex] -= quantity;
            orderNumber += 1;
            print("Order " + orderNumber + " total: $" + total);
            transition to Inventory;
        }
    }
}

// Operator, nesting, and literal edge cases kept together for token inspection.
int arithmeticProbe = ((orderNumber + 5) * 3) / 2 % 7;
int integerDivisionProbe = 19 \ 4;
int exponentProbe = 2 ^ 8;
bool comparisonProbe = (arithmeticProbe != 0) && (stockLevels[0] >= 0);
bool fallbackProbe = !(storeOpen == false) || comparisonProbe;
int updateProbe = 12;
updateProbe += 3;
updateProbe -= 1;
updateProbe *= 2;
updateProbe /= 7;
updateProbe %= 5;
float precisionProbe = 0.125;
int negativeProbe = -42;
string emptyText = "";
char characterProbe = 'Q';
string pipelineProbe = registerName |> normalizeName();

// Delimiters below are intentionally mismatched. The lexer should emit the
// bracket tokens; detecting the unmatched '[' is a parser responsibility.
print(productNames[0);

// Recoverable lexical errors: each should produce TOKEN_ERROR as appropriate.
@
#
1stPlace
thisIdentifierNameIsDefinitelyLongerThanThirtyTwoCharacters
char invalidCharacter = 'AB';

// Keep the unterminated string last: scanning it reaches the end of the file.
string unterminatedText = "This string never closes
