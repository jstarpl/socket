import main from './opkit.js'

setTimeout(() => {

let counter = 0

console.log('started')

main.setSize({ height: 500, width: 750 })
main.setTitle('Hello Operator')

main.setMenu(`
  App:
    About Operator: _
    ---: _
    Preferences...: , + Command
    ---: _
    Hide: h
    Hide Others: h + Control, Command
    ---: _
    Quit Operator: q + Command;

  Edit:
    Cut: x
    Copy: c
    Paste: v
    Delete: _
    Select All: a;

  Foo:
    Bazz: z + Command
    ---: _
    Quxx: e + ControlOrCommand, Option;

  Other:
    Another Test: t
    Beep: T + Command
`)


main.receive(async data => {
  return {
    received: data,
    counter: counter++
  }
})

setInterval(() => {
  counter++
 
  // send an odd sized message
  const size = Math.floor(Math.random() * 1e3)
  const data = new Array(size).fill(0)

  main.send({ sending: data.join(''), counter })
}, 5090) // send at some interval

process.on('beforeExit', () => {
  console.log('exiting')
})

}, 128)
