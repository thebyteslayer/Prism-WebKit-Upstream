function foo(a, b) {
    return a >> b;
}

noInline(foo);

var things = [{valueOf: function() { return -4; }}];
var results = [-2];

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo(things[i % things.length], 1);
    var expected = results[i % results.length];
    if (result != expected)
        throw "Error: bad result for i = " + i + ": " + result;
}

