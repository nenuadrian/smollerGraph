// A graph drawn on a canvas and laid out by D3's force simulation: a circle
// for each node, in its label's colour, and an arrow for each relationship. A
// canvas is one element that we paint pixels on, a frame at a time, where SVG
// keeps an element for each circle and line, for the browser to lay out and
// draw.
const RADIUS = 9;
// Names are written under the nodes while there are few enough of them to
// read, or once the view is zoomed in that far.
const NAMES_UP_TO = 150;

// A colour for each label: the series' chartreuse for species, a colour each
// for the ranks above them, and the rest for other labels, in the order they
// turn up. A node without a label is grey.
const colour =
    d3.scaleOrdinal(['Species', 'Genus', 'Family', 'Order', 'Class'], [
      '#c8f169', '#69c8f1', '#f1c869', '#c88cf1', '#6bf1b4', '#f1a0c8',
      '#a0f1e0'
    ]);
export function colourOf(label) {
  return label ? colour(label) : '#7e978d';
}

// The layout, which the page and the benchmark share. Relationships pull
// their nodes together, nodes push each other apart and keep out of each
// other's way, and the whole graph is pulled towards the middle.
export function layout() {
  return d3.forceSimulation()
      .force('link', d3.forceLink().id((d) => d.id).distance(50))
      .force('charge', d3.forceManyBody().strength(-200))
      .force('collide', d3.forceCollide(RADIUS * 2))
      .force('x', d3.forceX())
      .force('y', d3.forceY());
}

export class GraphView {
  // Draws on `canvas`, and calls onSelect with a node when it's clicked, and
  // onExpand with a node when it's double-clicked.
  constructor(canvas, onSelect, onExpand) {
    this.canvas = canvas;
    this.context = canvas.getContext('2d');
    this.nodes = [];
    this.links = [];
    this.selected = null;
    this.transform = d3.zoomIdentity;
    this.frameMs = 0;
    // Each tick of the layout paints a frame, and once the layout settles, the
    // view zooms to fit it.
    this.simulation =
        layout().on('tick', () => this.draw()).on('end', () => this.fit());

    // The canvas has as many pixels as the screen does, which is twice its
    // size in CSS pixels on many laptops, so circles and text stay sharp.
    const resize = () => {
      const ratio = window.devicePixelRatio;
      canvas.width = canvas.clientWidth * ratio;
      canvas.height = canvas.clientHeight * ratio;
      this.draw();
    };
    new ResizeObserver(resize).observe(canvas);

    // Dragging the background moves the view, and the wheel zooms it. Dragging
    // a node moves the node, which the simulation lets go of when it's
    // dropped.
    this.zoom = d3.zoom().on('zoom', (event) => {
      this.transform = event.transform;
      this.draw();
    });
    const drag = d3.drag()
                     .subject((event) => this.nodeAt(event.sourceEvent))
                     .on('start',
                         (event) => {
                           this.simulation.alphaTarget(0.3).restart();
                           event.subject.fx = event.subject.x;
                           event.subject.fy = event.subject.y;
                         })
                     .on('drag',
                         (event) => {
                           const [x, y] = this.transform.invert(
                               d3.pointer(event.sourceEvent, canvas));
                           event.subject.fx = x - canvas.clientWidth / 2;
                           event.subject.fy = y - canvas.clientHeight / 2;
                         })
                     .on('end', (event) => {
                       this.simulation.alphaTarget(0);
                       event.subject.fx = null;
                       event.subject.fy = null;
                     });
    d3.select(canvas)
        .call(drag)
        .call(this.zoom)
        .on('dblclick.zoom', null)
        .on('click',
            (event) => {
              const node = this.nodeAt(event);
              if (node) onSelect(node);
            })
        .on('dblclick', (event) => {
          const node = this.nodeAt(event);
          if (node) onExpand(node);
        });
  }

  // The node under the pointer, if there's one: D3's find goes through the
  // nodes for the nearest one within twice a node's radius.
  nodeAt(event) {
    const [x, y] = this.transform.invert(d3.pointer(event, this.canvas));
    return this.simulation.find(
        x - this.canvas.clientWidth / 2, y - this.canvas.clientHeight / 2,
        RADIUS * 2);
  }

  // Shows these nodes and relationships, in place of the ones shown, or
  // alongside them with `keep`. Nodes that stay keep their places.
  show(nodes, relationships, keep = false) {
    const old = new Map(this.nodes.map((d) => [d.id, d]));
    const byId = new Map(keep ? old : []);
    for (const node of nodes) byId.set(node.id, old.get(node.id) ?? {...node});
    this.nodes = [...byId.values()];
    const links = new Map(keep ? this.links.map((d) => [d.id, d]) : []);
    for (const d of relationships) {
      if (byId.has(d.from) && byId.has(d.to)) {
        links.set(d.id, {...d, source: d.from, target: d.to});
      }
    }
    this.links = [...links.values()];
    this.simulation.nodes(this.nodes);
    this.simulation.force('link').links(this.links);
    this.simulation.alpha(1).restart();
  }

  // Zooms the view so the graph fills it, with a margin, without zooming in
  // further than twice its size.
  fit() {
    if (this.nodes.length === 0) return;
    const [x0, x1] = d3.extent(this.nodes, (d) => d.x);
    const [y0, y1] = d3.extent(this.nodes, (d) => d.y);
    const width = this.canvas.clientWidth;
    const height = this.canvas.clientHeight;
    const k = Math.min(
        2, 0.8 * Math.min(width / (x1 - x0 + 1), height / (y1 - y0 + 1)));
    // The middle of the graph goes to the middle of the canvas, where the
    // simulation's 0, 0 is drawn.
    const x = width / 2 - k * ((x0 + x1) / 2 + width / 2);
    const y = height / 2 - k * ((y0 + y1) / 2 + height / 2);
    d3.select(this.canvas)
        .transition()
        .duration(600)
        .call(this.zoom.transform, d3.zoomIdentity.translate(x, y).scale(k));
  }

  select(node) {
    this.selected = node;
    this.draw();
  }

  // Paints a frame: the relationships first, as a path of lines and one of
  // arrowheads, then the nodes, a path for each label, as each path is one
  // call for the canvas to fill or stroke, however many shapes it holds. Then
  // the names, when they can be read.
  draw() {
    const start = performance.now();
    const {context, canvas, transform} = this;
    const ratio = window.devicePixelRatio;
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, canvas.clientWidth, canvas.clientHeight);
    context.translate(transform.x, transform.y);
    context.scale(transform.k, transform.k);
    context.translate(canvas.clientWidth / 2, canvas.clientHeight / 2);

    context.beginPath();
    for (const d of this.links) {
      context.moveTo(d.source.x, d.source.y);
      context.lineTo(d.target.x, d.target.y);
    }
    context.strokeStyle = 'rgba(255, 140, 107, 0.55)';
    context.lineWidth = 1.2;
    context.stroke();
    // An arrowhead at the edge of each node a relationship goes to.
    context.beginPath();
    for (const d of this.links) {
      const angle =
          Math.atan2(d.target.y - d.source.y, d.target.x - d.source.x);
      const x = d.target.x - Math.cos(angle) * RADIUS;
      const y = d.target.y - Math.sin(angle) * RADIUS;
      context.moveTo(x, y);
      context.lineTo(
          x - 7 * Math.cos(angle - 0.4), y - 7 * Math.sin(angle - 0.4));
      context.lineTo(
          x - 7 * Math.cos(angle + 0.4), y - 7 * Math.sin(angle + 0.4));
    }
    context.fillStyle = '#ff8c6b';
    context.fill();

    for (const [label, nodes] of d3.group(this.nodes, (d) => d.label)) {
      context.beginPath();
      for (const d of nodes) {
        context.moveTo(d.x + RADIUS, d.y);
        context.arc(d.x, d.y, RADIUS, 0, 2 * Math.PI);
      }
      context.fillStyle = colourOf(label);
      context.fill();
    }
    if (this.selected) {
      context.beginPath();
      context.arc(this.selected.x, this.selected.y, RADIUS + 4, 0, 2 * Math.PI);
      context.strokeStyle = '#dfe9e4';
      context.lineWidth = 2;
      context.stroke();
    }

    if (this.nodes.length <= NAMES_UP_TO || transform.k >= 2) {
      context.font = '12px "JetBrains Mono", monospace';
      context.textAlign = 'center';
      context.lineWidth = 4;
      context.strokeStyle = '#0a0f0e';
      context.fillStyle = '#dfe9e4';
      for (const d of this.nodes) {
        context.strokeText(d.name, d.x, d.y + RADIUS + 14);
        context.fillText(d.name, d.x, d.y + RADIUS + 14);
      }
    }
    // A moving average of how long a frame takes to paint, for the status
    // line.
    this.frameMs = 0.9 * this.frameMs + 0.1 * (performance.now() - start);
  }
}
