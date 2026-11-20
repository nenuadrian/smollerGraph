// The browser build's module, sg.js, loaded in Node as a page loads it: it
// builds the graph from the Wikidata facts, and answers as sg serve does.
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

// A second module, as a second page has, builds the made-up company, where
// 105 accounts have a path to Domain Admins.
const company = await load();
company.build('/graph', '/company.txt', 'MemberOf');
const paths = JSON.parse(company.query(
    'MATCH p = shortestPath((u:User)-[:MemberOf|AdminTo|HasSession|' +
    'ForceChangePassword|GenericAll*]->(g:Group {name: \'Domain Admins\'})) ' +
    'RETURN u.name, length(p)'));
assert.equal(paths.count, 105);

console.log(
    `${built.nodes} nodes and ${built.relationships} relationships, ` +
    `with their embeddings, in ${built.ms} ms`);
