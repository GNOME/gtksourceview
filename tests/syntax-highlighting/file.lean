-- line comment

/-
  block comment
  /- nested block comment -/
-/

def main : IO Unit :=
  let a := 1
  let a1 := 0b1
  let a₂ := 0x1
  let a' := '1'
  IO.println "Hello World"

#eval main
