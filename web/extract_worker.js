// Turns the CHD the user picked into the game library, in the browser's private storage.
importScripts('piu_extract.js');

const LIBRARY = 'piu-library';
const MARKER = 'library.json';
const LIBRARY_VERSION = 2;
const SOUNDS = 'piu-sfx';
const SOUNDS_MARKER = 'sounds.json';

async function wipe(root, name = LIBRARY) {
  try {
    await root.removeEntry(name, { recursive: true });
  } catch (error) {
    if (error.name !== 'NotFoundError') throw error;
  }
}

async function writeFile(library, folders, name, bytes) {
  const slash = name.lastIndexOf('/');
  let directory = library;
  if (slash >= 0) {
    const folder = name.slice(0, slash);
    directory = folders.get(folder);
    if (!directory) {
      directory = await library.getDirectoryHandle(folder, { create: true });
      folders.set(folder, directory);
    }
  }
  const handle = await directory.getFileHandle(name.slice(slash + 1), { create: true });
  const access = await handle.createSyncAccessHandle();
  try {
    access.truncate(0);
    const written = access.write(bytes, { at: 0 });
    if (written !== bytes.length) throw new Error('short write');
    access.flush();
  } finally {
    access.close();
  }
}

async function extract(file) {
  const extractor = await createExtractor();
  const failure = () => extractor.UTF8ToString(extractor._ext_error());
  self.piuDisc = file;

  // A wrong file stops here, before the library that already works is touched.
  if (!extractor._ext_open()) throw new Error(failure());

  // The marker goes in last, so a half-finished library never counts as ready.
  const root = await navigator.storage.getDirectory();
  await wipe(root);
  const library = await root.getDirectoryHandle(LIBRARY, { create: true });

  const folders = new Map();
  let count = 0;
  let announced = -1;
  while (extractor._ext_next()) {
    const pointer = extractor._ext_data();
    const name = extractor.UTF8ToString(extractor._ext_name());
    await writeFile(library, folders, name, extractor.HEAPU8.subarray(pointer, pointer + extractor._ext_size()));
    ++count;
    const percent = Math.floor(extractor._ext_progress() * 100);
    if (percent !== announced) {
      announced = percent;
      postMessage({ type: 'progress', percent });
    }
  }
  const message = failure();
  if (message) throw new Error(message);

  const marker = new TextEncoder().encode(JSON.stringify({
    version: LIBRARY_VERSION, source: file.name, files: count, finished: Date.now(),
  }));
  await writeFile(library, folders, MARKER, marker);
  postMessage({ type: 'done' });
}

async function extractSounds(file) {
  const root = await navigator.storage.getDirectory();
  const library = await root.getDirectoryHandle(LIBRARY);
  const table = await (await (await library.getFileHandle('piu.bin')).getFile()).arrayBuffer();
  const rom = await file.arrayBuffer();

  const extractor = await createExtractor();
  const copy = bytes => {
    const pointer = extractor._ext_alloc(bytes.byteLength);
    extractor.HEAPU8.set(new Uint8Array(bytes), pointer);
    return pointer;
  };
  const count = extractor._ext_sounds_open(copy(table), table.byteLength, copy(rom), rom.byteLength);
  if (!count) throw new Error('wrong-sound-rom');

  await wipe(root, SOUNDS);
  const sounds = await root.getDirectoryHandle(SOUNDS, { create: true });
  const folders = new Map();
  for (let i = 0; i < count; ++i) {
    const pointer = extractor._ext_sound_data(i);
    await writeFile(sounds, folders, `${i}.wav`, extractor.HEAPU8.subarray(pointer, pointer + extractor._ext_sound_size(i)));
    postMessage({ type: 'progress', percent: Math.floor((i + 1) * 100 / count) });
  }
  await writeFile(sounds, folders, SOUNDS_MARKER, new TextEncoder().encode(JSON.stringify({ count })));
  postMessage({ type: 'done' });
}

onmessage = async ({ data }) => {
  try {
    await (data.job === 'sounds' ? extractSounds(data.file) : extract(data.file));
  } catch (error) {
    postMessage({ type: 'error', message: error.message || String(error) });
  }
};
