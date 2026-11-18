// The browser build's module, sg.js, loaded in Node as a page loads it: it
// builds the graph from the Wikidata facts, and answers as sg serve does.
// Emscripten's loader reads sg.data from the folder it runs in, which for
// ctest is the build's.
import assert from 'node:assert/strict';
import {pathToFileURL} from 'node:url';

const {default: load} = await import(pathToFileURL(process.argv[2]));
const sg = await load();

const built = JSON.parse(sg.build('/graph', '/wikidata.txt', 'PARENT_TAXON'));
assert.ok(built.nodes > 20000, `only ${built.nodes} nodes`);

const answer = JSON.parse(sg.query(
    'MATCH (s:Species {name: \'Red fox\'})-[:PARENT_TAXON]->(g) RETURN g.name'));
assert.deepEqual(answer.rows.map((row) => row[0].value), ['Vulpes']);

assert.deepEqual(
    JSON.parse(sg.names('Red f')).names, ['Red forest duiker', 'Red fox']);

const genus = answer.rows[0][0].id;
assert.equal(JSON.parse(sg.node(`${genus}`)).name, 'Vulpes');
assert.equal(JSON.parse(sg.similar(`${genus}`)).similar.length, 10);

// A query it can't parse is an error, as sg serve sends it.
assert.match(JSON.parse(sg.query('MATCH (n RETURN n')).error, /expected \)/);

console.log(
    `${built.nodes} nodes and ${built.relationships} relationships, ` +
    `with their embeddings, in ${built.ms} ms`);
