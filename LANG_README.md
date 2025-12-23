# This is the basic interpreter for bolt
// =============================================
// Bolt Language Syntax Reference
// =============================================

// 1. DATA TYPES AND LITERALS
true;          // Boolean true
false;         // Boolean false
1234;          // Number (double-precision)
12.34;         // Floating-point number
"Hello";       // String literal
nil;           // Nil value

// 2. EXPRESSIONS AND OPERATORS

// Arithmetic operators
1 + 2;         // Addition
3 - 4;         // Subtraction
5 * 6;         // Multiplication
7 / 8;         // Division
-9;            // Negation (unary)

// Comparison operators (return booleans)
1 < 2;         // Less than
3 <= 4;        // Less than or equal
5 > 6;         // Greater than
7 >= 8;        // Greater than or equal
1 == 1;        // Equality
1 != 2;        // Inequality

// Logical operators
!true;         // Logical NOT
true and false;// Logical AND (short-circuiting)
true or false; // Logical OR (short-circuiting)

// Precedence and grouping
(1 + 2) * 3;   // Parentheses override precedence

// 3. STATEMENTS

// Print statement
print "Hello, world!";

// Expression statement
"some expression";

// Variable declaration
var x = 10;
var y;          // Defaults to nil

// Block statement
{
  print "One";
  print "Two";
}

// 4. CONTROL FLOW

// If statement
if (condition) {
  print "true";
} else {
  print "false";
}

// While loop
var i = 0;
while (i < 10) {
  print i;
  i = i + 1;
}

// For loop
for (var i = 0; i < 10; i = i + 1) {
  print i;
}

// 5. FUNCTIONS

// Function declaration
fun sayHello(name) {
  print "Hello, " + name + "!";
}

// Function with return value
fun add(a, b) {
  return a + b;
}

// First-class functions
var multiply = fun(a, b) { return a * b; };

// Closure example
fun makeCounter() {
  var i = 0;
  fun count() {
    i = i + 1;
    return i;
  }
  return count;
}

var counter = makeCounter();
print counter(); // 1
print counter(); // 2

// 6. CLASSES AND INHERITANCE

// Class declaration
class Breakfast {
  init(meat, bread) { // Constructor
    this.meat = meat;
    this.bread = bread;
  }
  
  serve(who) {
    print "Enjoy your " + this.meat + " and " + this.bread + ", " + who + ".";
  }
}

// Creating instances
var baconAndToast = Breakfast("bacon", "toast");
baconAndToast.serve("Bob");

// Inheritance
class Brunch < Breakfast {
  init(meat, bread, drink) {
    super.init(meat, bread);
    this.drink = drink;
  }
  
  serve(who) {
    super.serve(who);
    print "Have some " + this.drink + " too.";
  }
}

var benedict = Brunch("ham", "english muffin", "coffee");
benedict.serve("Cindy");

// 7. OPERATOR PRECEDENCE TABLE (HIGHEST TO LOWEST)
//    !, - (unary)      - Right associative
//    *, /              - Left associative
//    +, -              - Left associative
//    ==, !=, <, <=, >, >= - Left associative
//    and               - Left associative
//    or                - Left associative

// 8. LIMITATIONS AND OMISSIONS
// - No bitwise, shift, modulo, or conditional operators
// - No native array/list syntax (in base implementation)
// - No implicit type conversions
// - No +=, -=, etc. compound assignment operators
// - No ++, -- increment/decrement operators

// 9. NOTES
// - Variables are dynamically typed
// - Strings support concatenation with +
// - Garbage collection is automatic
// - All values are passed by reference
// - Functions can return implicitly (return nil)
// - Classes support single inheritance only
