// Draws a made-up graph of people, each knowing two others, with SVG, on a
// canvas, or not at all, while the page's layout moves it, and counts the
// frames the browser paints in three seconds. bench/ui.py opens it at each
// size, as bench.html?draw=canvas&nodes=1000, and reads window.result.
import {GraphView, layout} from './view.js';

const parameters = new URLSearchParams(location.search);
const draw = parameters.get('draw');
const count = Number(parameters.get('nodes'));

// The same graph each time, from a random number generator with a seed.
const random = d3.randomLcg(42);
const nodes = d3.range(count).map((id) => ({id, name: `Person ${id}`}));
const relationships = [];
for (const node of nodes) {
  for (let i = 0; i < 2; ++i) {
    const to = Math.floor(random() * count);
    relationships.push({id: relationships.length, from: node.id, to});
  }
}

if (draw === 'canvas') {
  const view = new GraphView(
      d3.select('body').append('canvas').node(), () => {}, () => {});
  view.show(nodes, relationships);
} else {
  // SVG, as the page drew the graph until now: an element for each line and
  // each circle, which the browser lays out and paints, or no drawing at all,
  // for what the layout costs on its own.
  const svg = d3.select('body').append('svg');
  const {width, height} = svg.node().getBoundingClientRect();
  const view = svg.append('g').attr(
      'transform', `translate(${width / 2}, ${height / 2})`);
  const links =
      relationships.map((d) => ({...d, source: d.from, target: d.to}));
  const line = view.selectAll('line')
                   .data(draw === 'svg' ? links : [])
                   .join('line')
                   .attr('stroke', '#ff8c6b');
  const circle = view.selectAll('circle')
                     .data(draw === 'svg' ? nodes : [])
                     .join('circle')
                     .attr('r', 9)
                     .attr('fill', '#c8f169');
  const simulation = layout().nodes(nodes);
  simulation.force('link').links(links);
  simulation.on('tick', () => {
    line.attr('x1', (d) => d.source.x)
        .attr('y1', (d) => d.source.y)
        .attr('x2', (d) => d.target.x)
        .attr('y2', (d) => d.target.y);
    circle.attr('cx', (d) => d.x).attr('cy', (d) => d.y);
  });
}

// The browser calls requestAnimationFrame's callback before it paints each
// frame, so counting the calls counts the frames.
let frames = 0;
const start = performance.now();
function countFrame(now) {
  if (now - start < 3000) {
    ++frames;
    requestAnimationFrame(countFrame);
    return;
  }
  window.result = {frames_per_second: frames / ((now - start) / 1000)};
}
requestAnimationFrame(countFrame);
