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

// The browser build has sg.js beside the page, and sg serve doesn't, so there
// the import fails, and the page asks the server.
async function inPage() {
  let sg;
  try {
    sg = await (await import('./sg.js')).default();
  } catch {
    return null;
  }
  sg.build('/graph', '/wikidata.txt', 'PARENT_TAXON');
  return {
    where: 'in the browser',
    query: async (q) => JSON.parse(sg.query(q)),
    names: async (prefix) => JSON.parse(sg.names(prefix)),
    node: async (id) => JSON.parse(sg.node(`${id}`)),
    similar: async (id) => JSON.parse(sg.similar(`${id}`)),
  };
}

export const api = (await inPage()) ?? overHttp;
