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

