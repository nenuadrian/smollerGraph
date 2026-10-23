// The page: a query in our subset of Cypher, run by sg serve, with its answer
// as a graph on a canvas, as a table and as a plan, a history of the queries
// run, a search for a node by its name, and an inspector that shows a node's
// record as it's kept on disk.
import {colourOf, GraphView} from './view.js';

const view =
    new GraphView(document.querySelector('#graph canvas'), inspect, expand);

// A number of things, such as 1 row or 307 rows.
function count(number, thing) {
  return `${number} ${thing}${number === 1 ? '' : 's'}`;
}

// A value as Cypher writes it: a number as it is, and a string in quotes,
// double ones when it holds a single one, as Pallas's cat does.
function literal(value) {
  if (typeof value !== 'string') return `${value}`;
  return value.includes('\'') ? `"${value}"` : `'${value}'`;
}

function showTab(name) {
  d3.selectAll('#tabs button').classed('active', function() {
    return this.value === name;
  });
  d3.selectAll('.tab').classed('active', function() {
    return this.id === name;
  });
}
d3.selectAll('#tabs button').on('click', function() {
  showTab(this.value);
});

// Running a query asks for its answer and, at the same time, for its plan, so
// the Plan tab always has the plan of the query that's shown.
const queryForm = d3.select('#query');
const text = queryForm.select('textarea');
const history = [];
// The status line's frame time comes a second after a query's answer, once a
// few frames have been painted.
let frameTimer = null;
async function run(q) {
  const ask = (query) => fetch(`/api/query?${new URLSearchParams({q: query})}`)
                             .then((response) => response.json());
  const explain = q.trim().toUpperCase().startsWith('EXPLAIN');
  const [answer, planned] =
      await Promise.all([ask(q), explain ? null : ask(`EXPLAIN ${q}`)]);
  const status = d3.select('#status').classed('error', Boolean(answer.error));
  if (answer.error) {
    status.text(answer.error);
    return;
  }
  d3.select('#plan pre').text(answer.plan ?? planned.plan ?? '');
  if (explain) {
    showTab('plan');
    status.text('');
    return;
  }
  remember(q);
  showTable(answer);
  view.show(answer.nodes, answer.relationships);
  showLegend();
  // How much of the answer there is, and what it cost: on the server, the
  // time from parsing to the last row, and the pages read from disk, and in
  // the browser, the time a frame takes to paint.
  const more = answer.count > answer.rows.length ?
      ` (the first ${answer.rows.length})` :
      '';
  const summary = `${count(answer.count, 'row')}${more} · ` +
      `${count(answer.nodes.length, 'node')}, ` +
      `${count(answer.relationships.length, 'relationship')} drawn · ` +
      `${answer.ms.toFixed(2)} ms and ${count(answer.pages, 'page')} ` +
      'on the server';
  status.text(summary);
  frameTimer?.stop();
  frameTimer = d3.timeout(() => {
    status.text(`${summary} · ${view.frameMs.toFixed(1)} ms a frame`);
  }, 1000);
}
queryForm.on('submit', (event) => {
  event.preventDefault();
  run(text.property('value'));
});
// Ctrl or Cmd and Enter runs the query, as in Neo4j Browser.
text.on('keydown', (event) => {
  if (event.key === 'Enter' && (event.ctrlKey || event.metaKey)) {
    event.preventDefault();
    run(text.property('value'));
  }
});

// The queries run, newest first, each once. Clicking one runs it again.
function remember(q) {
  const at = history.indexOf(q);
  if (at >= 0) history.splice(at, 1);
  history.unshift(q);
  d3.select('#history')
      .selectAll('li')
      .data(history)
      .join('li')
      .text((d) => d)
      .on('click', (event, d) => {
        text.property('value', d);
        run(d);
      });
}

// A key for the labels drawn, each in its colour, over the graph.
function showLegend() {
  d3.select('#graph .legend')
      .selectAll('li')
      .data([...new Set(view.nodes.map((d) => d.label))].sort())
      .join('li')
      .text((d) => d || 'no label')
      .style('--colour', (d) => colourOf(d));
}

function showTable(answer) {
  const table = d3.select('#table table');
  table.selectAll('*').remove();
  table.append('tr')
      .selectAll('th')
      .data(answer.columns)
      .join('th')
      .text((d) => d);
  table.selectAll('tr.row')
      .data(answer.rows)
      .join('tr')
      .attr('class', 'row')
      .selectAll('td')
      .data((d) => d)
      .join('td')
      .text((d) => d.value === null ? 'null' : d.value);
}

// As a name is typed, sg serve suggests the names that start with it, from its
// index. Picking one shows that node and its record.
const search = d3.select('#search');
const searchInput = search.select('input');
const suggestions = search.select('ul');
searchInput.on('input', async () => {
  const prefix = searchInput.property('value');
  const answer = prefix ?
      await (await fetch(`/api/names?${new URLSearchParams({prefix})}`))
          .json() :
      {names: []};
  suggestions.selectAll('li')
      .data(answer.names)
      .join('li')
      .text((d) => d)
      .on('click', (event, d) => {
        suggestions.selectAll('li').remove();
        searchInput.property('value', '');
        const q = `MATCH (n {name: ${literal(d)}}) RETURN n.name`;
        text.property('value', q);
        run(q).then(() => inspect(view.nodes[0]));
      });
});

// Double-clicking a node, or its inspector's button, draws the nodes its
// relationships go to.
d3.select('#inspector .expand').on('click', () => expand(view.selected));
async function expand(node) {
  const record = await (await fetch(`/api/node/${node.id}`)).json();
  view.show(
      record.chain.map((d) => ({id: d.to, name: d.to_name, label: d.to_label})),
      record.chain.map(
          (d) => ({id: d.id, from: node.id, to: d.to, type: d.type})),
      true);
  showLegend();
}

// Clicking a node shows its record as sg keeps it on disk: its label, where
// it is, its bytes, its chain of properties, and its chain of relationships.
async function inspect(node) {
  if (!node) return;
  view.select(node);
  const record = await (await fetch(`/api/node/${node.id}`)).json();
  const panel = d3.select('#inspector').classed('open', true);
  panel.select('h2').text(record.name);
  panel.select('.label')
      .text(record.label || 'no label')
      .style('color', colourOf(record.label));
  panel.select('.where').text(
      `Node ${record.id}: page ${record.page}, offset ${record.offset}`);
  panel.select('.bytes').text(record.bytes);
  panel.select('.properties')
      .selectAll('li')
      .data(record.properties)
      .join('li')
      .text(
          (d) => `${d.key}: ${literal(d.value)}\n` +
              `Property ${d.id}: page ${d.page}, offset ${d.offset}`);
  panel.select('.relationships')
      .selectAll('li')
      .data(record.chain)
      .join('li')
      .text(
          (d) => `${d.type} → ${d.to_name}\n` +
              `Relationship ${d.id}: page ${d.page}, offset ${d.offset}`);
}

run(text.property('value'));
