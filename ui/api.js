// What the page asks the graph: sg serve, over HTTP, or, in the browser
// build, the graph itself, running in the page as WebAssembly (src/web.cc).
// Both answer with the same JSON, and `where` says which it is.

const get = (url) => fetch(url).then((response) => response.json());

const overHttp = {
  where: 'on the server',
  query: (q) => get(`/api/query?${new URLSearchParams({q})}`),
  names: (prefix) => get(`/api/names?${new URLSearchParams({prefix})}`),
  node: (id) => get(`/api/node/${id}`),
  similar: (id) => get(`/api/similar/${id}`),
};

// The graphs the browser build can make from the facts packed beside it:
// Wikidata's mammals and birds, or, with ?graph=company in the page's address,
// a made-up company's Active Directory. Each has the relationship type its
// embeddings follow, and the company has a query of its own to open on.
const graphs = {
  wikidata: {facts: '/wikidata.txt', type: 'PARENT_TAXON'},
  company: {
    facts: '/company.txt',
    type: 'MemberOf',
    query: 'MATCH p = shortestPath((u:User)-[:MemberOf|AdminTo|HasSession|' +
        'ForceChangePassword|GenericAll*]->' +
        '(g:Group {name: \'Domain Admins\'})) RETURN u.name, length(p)',
  },
};

// The browser build has sg.js beside the page, and sg serve doesn't, so there
// the import fails, and the page asks the server.
async function inPage() {
  let sg;
  try {
    sg = await (await import('./sg.js')).default();
  } catch {
    return null;
  }
  const asked = new URLSearchParams(location.search).get('graph');
  const name = asked in graphs ? asked : 'wikidata';
  sg.build('/graph', graphs[name].facts, graphs[name].type);
  return {
    where: 'in the browser',
    graph: name,
    opening: graphs[name].query,
    query: async (q) => JSON.parse(sg.query(q)),
    names: async (prefix) => JSON.parse(sg.names(prefix)),
    node: async (id) => JSON.parse(sg.node(`${id}`)),
    similar: async (id) => JSON.parse(sg.similar(`${id}`)),
  };
}

export const api = (await inPage()) ?? overHttp;
