/* ==========================================
 THE TERMINAL FSM: SMART HOME DASHBOARD
 ========================================== */
// Language Documentation: Behavior, Semantics, and Control Flow

// 1. GLOBAL STATE
// Uses Type-First declarations with lowercase primitives. No 'let' or ':'.
bool lightsOn = false
int roomTemp = 22
int masterPin = 1234

// NEW: Array Declaration
[string] accessLogs = ["System Booted", "Thermostat Initialized"]

// 2. DATA-DRIVEN WATCHERS (react)
// Watchers trigger automatically when bound variables update via '='.
react (lightsOn) {
    // NEW: Ternary operator for concise conditional assignment
    string status = (lightsOn == true) ? "illuminated" : "extinguished"
    
    // NEW: String interpolation (F-string equivalent)
    print("[SYSTEM EVENT] Living room lights {status}.")
}

react (roomTemp) {
    print("[SYSTEM EVENT] Thermostat calibrated to {roomTemp}C.")
}

// 3. CUSTOM FUNCTIONS (fn)
// Functions explicitly declare their parameter types and return type.
fn formatStatus(bool status) -> string {
    return (status == true) ? "ACTIVE" : "INACTIVE"
}

fn removeSpaces(string text) -> string {
    // Assumes a built-in 'replace' utility
    return replace(text, " ", "")
}

// 4. ISOLATED STATES (state & transition)
// Execution requires explicit 'transition' routing. 'to' is an optional noise word.
state Boot {
    print("--- SYSTEM LOCKED ---")
    print("Enter 4-digit numeric PIN:")
    
    // 5. THE PIPELINE OPERATOR (|>)
    // Passes the left-hand result as the first argument to the right-hand function.
    int? input = readLine() |> removeSpaces() |> toInt()
    
    // 6. THE SAFE GATEWAY (ensure)
    // Safely unwraps an Optional into a strict primitive (Type-First).
    ensure int pin = input else {
        print("[ERROR] PIN must contain only numbers.")
        transition to Boot
    }
    
    if (pin == masterPin) {
        print("Access granted.")
        transition to Dashboard
    } else {
        print("Incorrect PIN.")
        transition to Boot
    }
}

state Dashboard {
    print("\n--- IOT MAIN MENU ---")
    print("1: Toggle Lights")
    print("2: Set Temperature")
    print("3: View System Status")
    print("4: Lock System")
    
    // 7. EXPLICIT LOOPING (loop)
    // Infinite blocking block broken only by a transition.
    loop {
        print("\nSelect an action (1-4):")
        string choice = readLine() |> removeSpaces()
        
        match choice {
            "1" -> transition to LightControl
            "2" -> transition to TempControl
            "3" -> transition to StatusView
            "4" -> transition to Boot
            ""  -> print("") 
            _   -> print("Unknown command.")
        }
    }
}

state LightControl {
    lightsOn = (lightsOn == true) ? false : true 
    transition to Dashboard
}

state TempControl {
    print("\nCurrent temperature is {roomTemp}C.")
    
    loop {
        print("Enter new target temperature (or 'cancel'):")
        
        string tempInput = readLine()
        
        if (tempInput == "cancel") {
            transition to Dashboard
        } else {
            int? newTemp = tempInput |> removeSpaces() |> toInt()
            
            ensure int validTemp = newTemp else {
                print("[ERROR] Temperature must be a number.")
                transition to TempControl 
            }
            
            // NEW: Single-line 'if' statements without curly braces
            if (validTemp < 16) print("Error: Minimum temperature is 16C.")
            else if (validTemp > 30) print("Error: Maximum temperature is 30C.")
            else {
                roomTemp = validTemp
                transition to Dashboard
            }
        }
    }
}

state StatusView {
    print("\n--- SYSTEM STATUS ---")
    
    string lightDisplay = lightsOn |> formatStatus()
    print("Lights: {lightDisplay}") 
    print("Thermostat: {roomTemp}C") 
    
    print("\n--- RECENT LOGS ---")
    // NEW: Array access and standard for-loop iteration
    for (int i = 0; i < length(accessLogs); i++) {
        print("- Log {i}: {accessLogs[i]}")
    }
    
    print("\nPress Enter to return to Dashboard.")
    string wait = readLine() 
    
    transition to Dashboard
}