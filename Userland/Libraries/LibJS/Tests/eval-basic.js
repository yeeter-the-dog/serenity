test("basic eval() functionality", () => {
    expect(eval("1 + 2")).toBe(3);

    function foo(a) {
        var x = 5;
        eval("x += a");
        return x;
    }
    expect(foo(7)).toBe(12);
});

test("returns value of last value-producing statement", () => {
    // See https://tc39.es/ecma262/#sec-block-runtime-semantics-evaluation
    expect(eval("")).toBeUndefined();
    expect(eval("1;;;;;")).toBe(1);
    expect(eval("1;{}")).toBe(1);
    expect(eval("1;var a;")).toBe(1);
});

test("syntax error", () => {
    expect(() => {
        eval("{");
    }).toThrowWithMessage(
        SyntaxError,
        "Unexpected token Eof. Expected CurlyClose (line: 1, column: 2)"
    );
});

test("returns 1st argument unless 1st argument is a string", () => {
    var stringObject = new String("1 + 2");
    expect(eval(stringObject)).toBe(stringObject);
});
var value="outer"
test("only touches locals if direct access", function() {
    var value = "inner"
    expect(global.eval("value")).toEqual("outer");
});

test("eval pointing to another function calls function", function() {
    var value = "inner"
    var eval = ()=>{return "wat"}
    expect(eval("value")).toEqual("wat");
});

test("eval pointing to eval works as a direct eval", function() {
    var value = "inner"
    var eval = global.eval
    expect(eval("value")).toEqual("inner");
});

test("alias to eval works as a global eval", function() {
    var value = "inner"
    var eval1 = global.eval
    expect(eval1("value")).toEqual("outer");
});
test("eval evaluates all args", function() {
    var i = 0
    expect(eval("value", i++, i++, i++)).toEqual("outer");
    expect(i).toEqual(3)
});

test("eval tests for exceptions", function() {
    var i = 0
    try {
        eval("value", i++, i++, j, i++)
        expect.unreached();
    } catch (e) {
        expect(i).toEqual(2)
    }
});
