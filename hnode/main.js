const fs = require('fs');
const vm = require('vm');

const fn = process.argv[1]

fs.readFile(fn, 'utf8', function (err, data) {
    new vm.Script(data, { filename: fn }).runInThisContext();
});
