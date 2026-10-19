// Read the actual settings presets and bridge mappings for emulator captures.
// Only isolated object literals are evaluated; neither application's code runs.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '..');
function objectLiteral(source, name) {
  const start = source.indexOf('var ' + name + ' = {');
  if (start < 0) throw new Error('Missing preset mapping: ' + name);
  const body = source.slice(source.indexOf('{', start), source.indexOf('};', start) + 1);
  return vm.runInNewContext('(' + body + ')', Object.create(null), {timeout: 1000});
}
const page = fs.readFileSync(path.join(root, 'resources/configure-fuzzy-text-two.html'), 'utf8');
const bridge = fs.readFileSync(path.join(root, 'src/js/pebble-js-app.js'), 'utf8');
const fields = [
  [1, 'text_align', 'alignments'], [3, 'font', 'fonts'],
  [4, 'foreground_colour', 'colourValues'], [5, 'background_colour', 'colourValues'],
  [6, 'date_position', 'datePositions'], [7, 'date_format', 'dateFormats'],
  [8, 'row_one_colour', 'optionalColourValues'], [9, 'row_two_colour', 'optionalColourValues'],
  [10, 'row_three_colour', 'optionalColourValues'], [11, 'date_part_one_colour', 'optionalColourValues'],
  [12, 'date_part_two_colour', 'optionalColourValues'], [13, 'date_part_three_colour', 'optionalColourValues'],
];
const result = {};
for (const [name, preset] of Object.entries(objectLiteral(page, 'presets'))) {
  result[name] = {};
  for (const [key, field, mapping] of fields) {
    const value = objectLiteral(bridge, mapping)[preset[field]];
    if (!Number.isInteger(value)) throw new Error('Invalid preset field: ' + name + '/' + field);
    result[name][key] = value;
  }
}
process.stdout.write(JSON.stringify(result));
