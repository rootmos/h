//require('child_process').execSync('echo "hello" 1>&2')
require('child_process').spawnSync('./hello', [], { env: [], stdio: 'inherit' })
