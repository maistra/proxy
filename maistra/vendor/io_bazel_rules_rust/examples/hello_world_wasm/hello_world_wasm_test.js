const fs = require('fs');
const path = require('path');

describe('Calling WebAssembly code', () => {
  it('Should run WebAssembly code', async () => {
    const buf = fs.readFileSync(path.join(__dirname, 'hello_world_wasm_bindgen_bg.wasm'));
    const res = await WebAssembly.instantiate(buf);
    expect(res.instance.exports.hello_world(2)).toEqual(4);
  })
});
