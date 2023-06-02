// This file should compile and execute

import static java.lang.System.out;

interface I {
    void foo();
    String[] bar(boolean okay);
}

abstract class C implements I {
    public static void println(String str) {
        out.println(str);
    }

    @Override
    public void foo() {}
}

enum E { YEAH }

/** Do you document your API?
 * @param fake Oh no!
 * @return What?
 */
record R<T>(int i, double d, char c, Object o, T special) {
    private static final byte thing = 0;
}

public final class /* the same as the file name */ file extends C {
    @Override
    public String[] bar(boolean okay) {
        return new String[] {
            "Float: " + 1f + " or " + 1.e+0f,
            "Double: " + 1d + " or " + 1.0e-0d,
            "Long: " + 1L + " or " + 0x1l,
            "Unsigned: don’t exist in Java",
            "Escaped chars: \\ \" \101 " + '\141',
            """
            Multiline string:
                - One
                - Two
                - Three""",
        };
    }

    public static void main(String[] args) {
        println(E.YEAH.toString());
        var me = new file();
        me.foo();
        for (var wysiwyg: me.bar(false)) {
            println(wysiwyg);
        }
        while (me != null) {
            me = null;
        }
        var what = switch ("hey") {
                case "" -> "";
                default -> { yield "Have fun\u0021"; }
            };
        println(what.toUpperCase());
    }
}
