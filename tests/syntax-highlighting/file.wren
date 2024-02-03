System.print("Hello, world!")

class Wren {
  flyTo(city) {
    System.print("Flying to %(city)")
  }
}

// https://wren.io/values.html#numbers
var numbers = [
  0, 1234, -5678, 3.14159, 1.0, -12.34,
  0.0314159e02, 0.0314159e+02, 314.159e-02, 0xcaffe2
]
var int = 12
var float = 0.004
// var invalidHex = 0xG
// var invalidOctal = 08u

var adjectives = Fiber.new {
  ["small", "clean", "fast"].each {|word| Fiber.yield(word) }
}

while (!adjectives.isDone) System.print(adjectives.call())
