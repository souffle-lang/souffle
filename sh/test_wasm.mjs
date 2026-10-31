// Smoke test for the WebAssembly build of Souffle (see the "WebAssembly" section of
// README.md). Run it with Node.js after building:
//
//     node sh/test_wasm.mjs build-wasm/src/souffle.js
//
// Each test runs a Datalog program through the module's callMain() on its in-memory file
// system, the way a web page would, and checks the output. Exits with status 1 if any test
// fails.

import { createRequire } from 'node:module'
import path from 'node:path'

const souffleJs = path.resolve(process.argv[2] ?? 'build-wasm/src/souffle.js')
const SOUFFLE = createRequire(import.meta.url)(souffleJs)

// Runs Souffle once, on a fresh module instance (callMain can only be called once per
// instance), with the given files written to its virtual file system first.
async function runSouffle(args, files = {}) {
  let stdout = '', stderr = ''
  const module = await SOUFFLE({
    noInitialRun: true,
    print: (line) => { stdout += line + '\n' },
    printErr: (line) => { stderr += line + '\n' },
  })
  for (const [name, contents] of Object.entries(files)) {
    const dir = path.posix.dirname(name)
    if (dir !== '.') module.FS.mkdirTree(dir)
    module.FS.writeFile(name, contents)
  }
  let status
  try {
    status = module.callMain(args)
  } catch (e) {
    if (e?.name !== 'ExitStatus') throw e
    status = e.status
  }
  return { status, stdout, stderr, module }
}

const tests = []
const test = (name, fn) => tests.push([name, fn])
const check = (condition, message) => { if (!condition) throw new Error(message) }

test('recursive program, output to stdout', async () => {
  const program = `
    .decl edge(x: number, y: number)
    edge(1, 2). edge(2, 3). edge(3, 4).
    .decl path(x: number, y: number)
    path(x, y) :- edge(x, y).
    path(x, z) :- path(x, y), edge(y, z).
    .output path
  `
  const { status, stdout } = await runSouffle(['--no-preprocessor', '-D-', 'program.dl'],
                                              { 'program.dl': program })
  check(status === 0, `exit status ${status}`)
  for (const tuple of ['1\t2', '1\t3', '1\t4', '2\t3', '2\t4', '3\t4']) {
    check(stdout.includes(tuple), `missing path ${tuple} in output:\n${stdout}`)
  }
})

test('facts from files, output to files', async () => {
  const program = `
    .decl person(name: symbol, age: number)
    .input person
    .decl adult(name: symbol)
    adult(n) :- person(n, a), a >= 18.
    .output adult
  `
  const { status, module } = await runSouffle(
    ['--no-preprocessor', '-F', 'facts', '-D', 'out', 'program.dl'],
    { 'program.dl': program, 'facts/person.facts': 'Alice\t30\nBob\t12\nCarol\t18\n', 'out/.keep': '' })
  check(status === 0, `exit status ${status}`)
  const output = module.FS.readFile('out/adult.csv', { encoding: 'utf8' })
  check(output.split('\n').filter(Boolean).sort().join(',') === 'Alice,Carol',
        `unexpected output: ${JSON.stringify(output)}`)
})

test('error in the program is reported', async () => {
  const { status, stderr } = await runSouffle(['--no-preprocessor', '-D-', 'program.dl'],
                                              { 'program.dl': 'result(x) :- nosuchrelation(x).' })
  check(status !== 0, 'expected a non-zero exit status')
  check(/undefined relation/i.test(stderr), `expected an error message, got:\n${stderr}`)
})

let failed = 0
for (const [name, fn] of tests) {
  try {
    await fn()
    console.log(`ok    ${name}`)
  } catch (e) {
    failed++
    console.log(`FAIL  ${name}: ${e.message}`)
  }
}
process.exit(failed ? 1 : 0)
