const fs = require('fs');
const vm = require('vm');

fs.readFile(input_script_filename, 'utf8', function(err, data) {
    new vm.Script(data, { filename: input_script_filename }).runInThisContext();
});
