# Resin
Resin is a simple programming language running on a bytecode VM. It is single pass and faster than Python!

# Examples
```java
print("Hello, world!");
```
```java
def fib(n) {
  if (n < 2) {
    return n;
  }
  return fib(n - 2) + fib(n - 1);
}

print(fib(35));
```
```java
class Math {
  // Not the best power function and
  // it breaks when you use a decimal.
  // Use the power operator ('^') instead.
  def pow(num, base) {
    if (base == 0) {
      return 1;
    }
    return num * this.pow(num, base - 1);
  }
  
  def log(base, num) {
    if (num <= base) {
      return 1;
    }
    return this.log(base, num / base) + 1;
  }
}

class Greet {
  def sayHello(name) {
    print("Hello, " + name + "!");
  }

  def sayHola(name) {
    print("Hola, " + name + "!");
  }
}

class Point {
  def init(coordinateX, coordinateY) {
    this.x = coordinateX;
    this.y = coordinateY;
  }
}

print(Math().pow(9, 5));
print(Math().log(10, 1000));
Greet().sayHello("John Doe");
Greet().sayHola("John Doe");

let testPoint = Point(7, 5);
print(testPoint.x);
print(testPoint.y);
```
```java
class A {
  def method() {
    print("A method");
  }
}

class B extends A {
  def method() {
    print("B method");
  }

  def test() {
    super.method();
  }
}

class C extends B {}

C().test();
```
```java
let testList = [
  "one", "two",
  "three", "four",
  "five", "six",
  "seven", "eight",
  "nine", "ten"
];

for (let i = 0; i < 10; i = i + 1) {
  print(testList[i]);
}
```
```java
let x = 3;

match (x) {
  with 0 -> print("zero");
  with 1 -> print("one");
  with 2 -> print("two");
  _ -> print("other");
}
```

*More in the `examples/` folder.*

*See benchmarks in the `bench/` folder.*

# Build Instructions
Just run `make` and then `make install`.

**NOTE: `make install` does not work on Windows at the moment.**

# Issues
Don't be afraid to create an issue if something goes wrong. I'd love to fix it!