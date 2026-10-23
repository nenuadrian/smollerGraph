// The graph sg serve sends, drawn with D3: a circle for each node and an arrow
// for each relationship, placed by a force simulation.
const svg = d3.select('#graph');
const {width, height} = svg.node().getBoundingClientRect();
const view = svg.append('g');

// Dragging the background moves the view, and the mouse wheel zooms it.
const zoom = d3.zoom().on('zoom', (event) => {
  view.attr('transform', event.transform);
});
svg.call(zoom);

const graph = await (await fetch('/api/graph')).json();
// D3 finds the two ends of a link by its source and target.
const links =
    graph.relationships.map((d) => ({...d, source: d.from, target: d.to}));

// Relationships pull their nodes together, nodes push each other apart, and
// the whole graph is pulled towards the middle.
const simulation =
    d3.forceSimulation(graph.nodes)
        .force('link', d3.forceLink(links).id((d) => d.id).distance(170))
        .force('charge', d3.forceManyBody().strength(-900))
        .force('center', d3.forceCenter(width / 2, height / 2))
        .force('collide', d3.forceCollide(60));

const link = view.selectAll('.relationship')
                 .data(links)
                 .join('g')
                 .attr('class', 'relationship');
link.append('line').attr('marker-end', 'url(#arrow)');
link.append('text').text((d) => d.type);

const node = view.selectAll('.node')
                 .data(graph.nodes)
                 .join('g')
                 .attr('class', 'node')
                 .call(drag(simulation));
node.append('circle').attr('r', 26);
node.append('text').attr('dy', 48).text((d) => d.name);

// Each tick of the simulation moves the nodes a little, and the relationships
// follow their ends.
simulation.on('tick', () => {
  link.select('line')
      .attr('x1', (d) => d.source.x)
      .attr('y1', (d) => d.source.y)
      .attr('x2', (d) => d.target.x)
      .attr('y2', (d) => d.target.y);
  link.select('text')
      .attr('x', (d) => (d.source.x + d.target.x) / 2)
      .attr('y', (d) => (d.source.y + d.target.y) / 2);
  node.attr('transform', (d) => `translate(${d.x}, ${d.y})`);
});

// Dragging a node pins it under the pointer, and lets it go when it's dropped.
function drag(simulation) {
  return d3.drag()
      .on('start',
          (event, d) => {
            if (!event.active) simulation.alphaTarget(0.3).restart();
            d.fx = d.x;
            d.fy = d.y;
          })
      .on('drag',
          (event, d) => {
            d.fx = event.x;
            d.fy = event.y;
          })
      .on('end', (event, d) => {
        if (!event.active) simulation.alphaTarget(0);
        d.fx = null;
        d.fy = null;
      });
}

// A walk asks sg serve for the nodes each hop reaches, and lights them up a
// hop at a time, with the relationships it followed to reach them.
const form = d3.select('#walk');
let timers = [];
form.on('submit', async (event) => {
  event.preventDefault();
  suggestions.selectAll('li').remove();
  const query = new URLSearchParams(new FormData(event.target));
  const response = await fetch(`/api/walk?${query}`);
  const answer = await response.json();
  form.select('output').text(answer.error ?? '');
  if (response.ok) light(answer.hops, query.get('type'));
});

// As a name is typed, sg serve suggests the names that start with it, from its
// index, and clicking one fills it in.
const from = form.select('input[name=from]');
const suggestions = form.select('.names ul');
from.on('input', async () => {
  const prefix = from.property('value');
  const query = new URLSearchParams({prefix});
  const answer =
      prefix ? await (await fetch(`/api/names?${query}`)).json() : {names: []};
  suggestions.selectAll('li')
      .data(answer.names)
      .join('li')
      .text((d) => d)
      .on('click', (event, d) => {
        from.property('value', d);
        suggestions.selectAll('li').remove();
      });
});

function light(hops, type) {
  // A walk started before this one stops lighting things up.
  timers.forEach(clearTimeout);
  node.classed('reached answer', false);
  link.classed('followed', false);
  timers = hops.map(
      (_, hop) => setTimeout(() => lightHop(hops, hop, type), hop * 1000));
}

// Lights up the nodes one hop reaches, and the relationships it followed from
// the hop before. The start has no hop before it, so it follows none.
function lightHop(hops, hop, type) {
  const before = new Set(hops[hop - 1]);
  const reached = new Set(hops[hop]);
  const followed = (d) =>
      d.type === type && before.has(d.source.id) && reached.has(d.target.id);
  link.filter(followed).classed('followed', true);
  node.filter((d) => reached.has(d.id))
      .classed('reached', true)
      .classed('answer', hop === hops.length - 1);
}

// Clicking a node shows its record as sg keeps it on disk: where it is, its
// bytes, its chain of properties, and its chain of relationships, which light
// up in the graph too.
node.on('click', (event, d) => inspect(d.id));

// A value as Cypher writes it: a string in quotes, and a number as it is.
function literal(value) {
  return typeof value === 'string' ? `'${value}'` : `${value}`;
}

// A relationship's properties in braces, as in a pattern, or nothing.
function braces(properties) {
  const pairs = Object.entries(properties).map(([key, value]) => {
    return `${key}: ${literal(value)}`;
  });
  return pairs.length ? ` {${pairs.join(', ')}}` : '';
}

async function inspect(id) {
  const record = await (await fetch(`/api/node/${id}`)).json();
  const panel = d3.select('#inspector');
  if (!panel.classed('open')) {
    // The panel takes room from the graph, so the view moves over by half of
    // it, and the graph stays in the middle of what's left.
    panel.classed('open', true);
    svg.transition().call(zoom.translateBy, -panel.node().offsetWidth / 2, 0);
  }
  panel.select('h2').text(record.name);
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
          (d) => `${d.type}${braces(d.properties)} → ${d.to_name}\n` +
              `Relationship ${d.id}: page ${d.page}, offset ${d.offset}`);
  const chain = new Set(record.chain.map((d) => d.id));
  node.classed('inspected', (d) => d.id === id);
  link.classed('chained', (d) => chain.has(d.id));
}

// A query in our subset of Cypher, which sg serve parses, plans and runs. Its
// rows go in a table, and the nodes they return are filled in, in the graph.
// After EXPLAIN, the table shows the plan instead.
const queryForm = d3.select('#query');
const results = d3.select('#results');
queryForm.on('submit', async (event) => {
  event.preventDefault();
  const query = new URLSearchParams(new FormData(event.target));
  const answer = await (await fetch(`/api/query?${query}`)).json();
  results.classed('open', true);
  results.select('.error').text(answer.error ?? '');
  results.select('.plan').text(answer.plan ?? '');
  const table = results.select('table');
  table.selectAll('*').remove();
  if (!answer.rows) return;
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
      .text((d) => d.value ?? 'null');
  // The query's nodes take the place of a walk's.
  timers.forEach(clearTimeout);
  const returned = new Set(answer.rows.flat().map((d) => d.id));
  link.classed('followed', false);
  node.classed('reached', false).classed('answer', (d) => returned.has(d.id));
});
