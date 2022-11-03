# Resin
Resin is a simple programming language running on a bytecode VM. It is single pass and faster than Python!

The latest version is 11-03-2022.

# Examples
```scala
println("Hello, world!")
```
```scala
func fib(n) {
  if (n < 2) {
    return n
  }
  return fib(n - 2) + fib(n - 1)
}

println(fib(35))
```
```scala
class Math {
  // Not the best power function and
  // it breaks when you use a decimal.
  // Use the power operator ('^') instead.
  func pow(num, base) {
    if (base == 0) {
      return 1
    }
    return num * this.pow(num, base - 1)
  }
  
  func log(base, num) {
    if (num <= base) {
      return 1
    }
    return this.log(base, num / base) + 1
  }
}

class Greet {
  func sayHello(name) {
    println("Hello, " + name + "!")
  }

  func sayHola(name) {
    println("Hola, " + name + "!")
  }
}

class Point {
  func init(coordinateX, coordinateY) {
    this.x = coordinateX
    this.y = coordinateY
  }
}

println(Math().pow(9, 5))
println(Math().log(10, 1000))
Greet().sayHello("John Doe")
Greet().sayHola("John Doe")

let testPoint = Point(7, 5)
println(testPoint.x)
println(testPoint.y)
```
```scala
// Stolen from Robert Nystrom

class A {
  func method() {
    println("A method")
  }
}

class B extends A {
  func method() {
    println("B method")
  }

  func test() {
    super.method()
  }
}

class C extends B {}

C().test()
```
```scala
let testList = [
  "one", "two",
  "three", "four",
  "five", "six",
  "seven", "eight",
  "nine", "ten"
]

print("List contents: ")
println(testList)

for (let i = 0; i < 10; i = i + 1) {
  println(testList[i])
}
```
```scala
let x = 3

match (x) {
  with 0 -> println("zero")
  with 1 -> println("one")
  with 2 -> println("two")
  _ -> println("other")
}
```

*More in the `examples/` folder.*

*See benchmarks in the `bench/` folder.*

# Build Instructions
Just run `make` and then `make install`.

**NOTE: `make install` does not work on Windows at the moment.**

# Issues
Don't be afraid to create an issue if something goes wrong. I'd love to fix it!