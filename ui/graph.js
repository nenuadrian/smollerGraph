// The graph sg serve sends, drawn with D3: a circle for each node and an arrow
// for each relationship, placed by a force simulation.
const svg = d3.select('#graph');
const {width, height} = svg.node().getBoundingClientRect();
const view = svg.append('g');

// Dragging the background moves the view, and the mouse wheel zooms it.
svg.call(d3.zoom().on('zoom', (event) => {
  view.attr('transform', event.transform);
}));

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
