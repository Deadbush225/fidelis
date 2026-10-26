// This is a single-line comment
/* This is a multi-line
   block comment spanning
   multiple lines */

int x = 42;
float temp = 98.6;
string name = "Fidelis";
char grade = 'A';
bool isActive = true;

if (x > 10 && temp <= 100.0) {
    print("Values are in range!");
}

fn calculate(int a, int b) -> float {
    return (a + b) / 2.0;
}

state Running {
    transition to Idle;
}

loop {
    string input = readLine();
    match input {
        "1" -> print("One");
        "2" -> print("Two");
        _ -> print("Other");
    }
}

// Comment with noise words
int value is = 5;
int x = ensure value else { print("Invalid, retry again"); transition to Running; };
state Boot do { print("Booting"); }
transition to Dashboard;

// Invalid lexical examples: these unsupported characters should produce TOKEN_ERROR.
@
#
~

// Invalid identifiers from the lexical evaluation example.
1stPlace
user-name
total sum
