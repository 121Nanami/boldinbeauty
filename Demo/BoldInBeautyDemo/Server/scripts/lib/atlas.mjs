// Thin wrapper around the MongoDB Atlas CLI (`atlas`, https://www.mongodb.com/docs/atlas/cli/). Every command and flag below was
// checked against `atlas --help` of CLI version 1.58.3. The CLI logs in with a one-time code the user approves in the browser;
// nothing here ever sees the user's Atlas password.
//
// `exec(command, args, { inherit })` is injected: the real one spawns a process, the tests use a scripted fake.

// The CLI may print warnings before its JSON ("Secure storage is not available..."): take the first JSON value in the output.
export function parseJson(text) {
  const start = text.search(/[[{]/);
  if (start < 0) throw new Error('the Atlas CLI printed no JSON');
  const opener = text[start];
  const closer = opener === '{' ? '}' : ']';
  const end = text.lastIndexOf(closer);
  if (end < start) throw new Error('the Atlas CLI printed incomplete JSON');
  return JSON.parse(text.slice(start, end + 1));
}

const list = (json) => (Array.isArray(json) ? json : Array.isArray(json?.results) ? json.results : []);

export class AtlasError extends Error {
  constructor(message, code) {
    super(message);
    this.code = code;
  }
}

export function createAtlas(exec, redact = (text) => text) {
  const fail = (what, result) => {
    const detail = redact(`${result.stderr || ''}${result.stdout || ''}`.trim()).split('\n').filter((line) => !/Secure storage|silence_storage_warning/.test(line)).join('\n');
    return new AtlasError(`${what} failed${detail ? `: ${detail}` : ''}`, result.code);
  };
  const run = async (what, args, options) => {
    const result = await exec('atlas', args, options);
    if (result.code !== 0) throw fail(what, result);
    return result;
  };
  const runJson = async (what, args) => parseJson((await run(what, args)).stdout);

  return {
    async installed() {
      return (await exec('atlas', ['--version'])).code === 0;
    },

    // { ok: true, text } when logged in.
    async whoami() {
      const result = await exec('atlas', ['auth', 'whoami']);
      return { ok: result.code === 0, text: `${result.stdout || ''}${result.stderr || ''}`.replace(/.*(Secure storage|silence_storage_warning).*\n?/g, '').trim() };
    },

    // Shows the one-time code and opens the browser; the user approves it there.
    async login() {
      await run('atlas auth login', ['auth', 'login'], { inherit: true });
    },

    async projects() {
      return list(await runJson('listing projects', ['projects', 'list', '-o', 'json'])).map((project) => ({ id: project.id, name: project.name }));
    },

    async createProject(name) {
      const project = await runJson('creating the project', ['projects', 'create', name, '-o', 'json']);
      return { id: project.id, name: project.name || name };
    },

    // Standard and Flex clusters are listed separately by the CLI; merge them.
    async clusters(projectId) {
      const found = new Map();
      for (const extra of [[], ['--tier', 'FLEX']]) {
        try {
          const json = await runJson('listing clusters', ['clusters', 'list', '--projectId', projectId, '-o', 'json', ...extra]);
          for (const cluster of list(json)) {
            if (!found.has(cluster.name)) {
              found.set(cluster.name, { name: cluster.name, state: cluster.stateName || cluster.state || '', srv: cluster.connectionStrings?.standardSrv || '' });
            }
          }
        } catch (error) {
          if (extra.length === 0) throw error; // the standard listing must work; the Flex one is best effort
        }
      }
      return [...found.values()];
    },

    async createCluster(projectId, name, { provider, region, tier }) {
      await run('creating the cluster', ['clusters', 'create', name, '--projectId', projectId, '--provider', provider, '--region', region, '--tier', tier]);
    },

    // Blocks (and prints progress) until the cluster is ready; a new cluster takes a few minutes.
    async watchCluster(projectId, name) {
      await run('waiting for the cluster', ['clusters', 'watch', name, '--projectId', projectId], { inherit: true });
    },

    // The password is passed on the command line (the CLI has no other non-interactive way): visible to other users of THIS computer
    // for a moment, which is acceptable on a personal machine; it is never printed or stored by this script except in .env.
    async createDbUser(projectId, { username, password, database, cluster }) {
      const args = ['dbusers', 'create', '--username', username, '--password', password, '--role', `readWrite@${database}`, '--projectId', projectId, '-o', 'json'];
      if (cluster) args.push('--scope', cluster);
      await run('creating the database user', args);
    },

    async deleteDbUser(projectId, username) {
      await run('removing the old database user', ['dbusers', 'delete', username, '--projectId', projectId, '--force']);
    },

    // Lets this computer's current public address connect to the cluster.
    async allowCurrentIp(projectId, comment) {
      const result = await exec('atlas', ['accessList', 'create', '--currentIp', '--projectId', projectId, '--comment', comment, '-o', 'json']);
      if (result.code !== 0 && !/already|duplicate|exist/i.test(`${result.stderr}${result.stdout}`)) throw fail('adding this computer to the access list', result);
    },

    // mongodb+srv://<cluster-host> without credentials.
    async srvHost(projectId, cluster) {
      const json = await runJson('reading the connection string', ['clusters', 'connectionStrings', 'describe', cluster, '--projectId', projectId, '-o', 'json']);
      const strings = json.connectionStrings || json;
      return strings.standardSrv || '';
    },
  };
}
