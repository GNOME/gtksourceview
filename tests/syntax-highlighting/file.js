// Regular expressions:
/abc/
x = /abc/;
function_with_regex_arg(/abc/);
[ /abc/, /def/];
{ regex: /abc/ };
(1 === 0) ? /abc/ : /def/;
/abc/ /* Comment */
/abc/ // Comment
var matches = /abc/.exec('Alphabet ... that should contain abc, right?');

// No regex here: 
a = [thing / thing, thing / thing];
x = a /b/ c / d;

// Template strings
// ----------------
// Template strings are delimited by back-ticks (grave accent) and
// can span multiple lines. They allow for expressions that inside
// of a dollar-sign plus curly-bracket construct (${...}).

console.log(`The sum of 2 and 2 is ${2 + 2}`);

let y = 8;
let my_string = `This is a multiline
string that also contains
a template ${y + (4.1 - 2.2)}`;
