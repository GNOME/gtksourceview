open Printf

(*comment
(** comment in comment *)
"string *)\nin comment"
*)

let id = fun x -> x

let whatelse _ ?_a ?(b = 1.) ~c () = (Option.value _a ~default:0.) +. b +. c

(**) (*<- empty comment*)
(**ocamldoc (*and comment*) *)

type thing =
| Thing1
| Thing2 of string option
| Thing3 of { first: string; second: string }

let _ =
    print_string "Type a number: ";
    let nb1 = read_float ()
    and nb2 = Random.float 10. in
    printf "%g × 2 is %g.\n" (id nb1) (nb1 *. 2.);
    printf "%g + %g is %g.\n" nb1 nb2 (whatelse () ~b:nb1 ~c:nb2 ());

    print_endline "Now some multiplication table:";
    for i1 = 1 to 10 do
        for i2 = 1 to 10 do
            printf "%3d " (i1 * i2);
        done;
        print_newline ();
    done;

    Array.iter (fun thing ->
            print_endline (
                match thing with
                | Thing1 -> "And 1."
                | Thing2 None -> "Or 2…"
                | Thing2 (Some str) -> "Or 2 with " ^ str ^ "!"
                | Thing3 pair -> "3 says:\n" ^ pair.first ^ "\nand:\n" ^ pair.second
            );
        ) [| Thing1; Thing2 None; Thing2 (Some "this"); Thing3 {
            first = "this is
\097
multiline string";
            second = {|I can easily write
backslashes because there is a syntax in which
\escapes \do \not \\\work|} ^ {x| (this syntax is {|string|})|x};
        } |];
