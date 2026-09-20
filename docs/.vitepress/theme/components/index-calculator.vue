<script lang="ts" setup>
import { computed, ref, watch } from 'vue'
import {
  NFormItem, NInputNumber, NButton, NDivider,
  NRadioGroup, NRadioButton
} from 'naive-ui';

const unit = ref('Kb'); // Default unit
const psramSize = ref(convert(64 * 1e6 / 8)); // 64 Mbits

const entities = ref([
  {
    value: 20000,
    label: 'Tracks',
    averageLength: 35,
    enabled: true,
    key: 'tracks'
  },
  {
    value: 1000,
    label: 'Albums',
    averageLength: 20,
    enabled: true,
    key: 'albums'
  },
  {
    value: 500,
    label: 'Artists',
    averageLength: 15,
    enabled: true,
    key: 'artists'
  },
  {
    value: 50,
    label: 'Genres',
    averageLength: 10,
    enabled: true,
    key: 'genres'
  }
]);

const relations = ref({
  genreToArtist: true,
  genreToAlbum: true,
  genreToTrack: true,
  artistToAlbum: true,
  albumToTracks: true,
  artistToTracks: true
});

function convert(n: number) {
  if (unit.value === 'Kb') {
    return Math.ceil(n / 1024);
  } else if (unit.value === 'Mb') {
    return +Number(n / 1024 / 1024).toFixed(3);
  } else {
    return n;
  }
}
function relationIndexSize(sources: number, targets: number) {
  // 8bits for the header + source id, offset, length => 3 uint32 per entity 
  return 8 + sources * (12) + 4 * targets;
}
function stringIndexSize(n: number, averageLength: number) {
  // 8bits for the header + n * (length + 12) => id, offset, length + string length
  return 8 + n * (averageLength + 12);
}
const getEntityByKey = (key: string) => entities.value.find(e => e.key === key);

const trackIndexSize = computed(() => {
  const tracks = getEntityByKey('tracks');
  return tracks ? stringIndexSize(tracks.value, tracks.averageLength) : 0;
});
const albumIndexSize = computed(() => {
  const albums = getEntityByKey('albums');
  return albums ? stringIndexSize(albums.value, albums.averageLength) : 0;
});
const artistIndexSize = computed(() => {
  const artists = getEntityByKey('artists');
  return artists ? stringIndexSize(artists.value, artists.averageLength) : 0;
});
const genreIndexSize = computed(() => {
  const genres = getEntityByKey('genres');
  return genres ? stringIndexSize(genres.value, genres.averageLength) : 0;
});
const indexSize = computed(() => {
  const tracks = getEntityByKey('tracks');
  const albums = getEntityByKey('albums');
  const artists = getEntityByKey('artists');
  const genres = getEntityByKey('genres');

  return (tracks?.enabled ? trackIndexSize.value : 0)
    + (albums?.enabled ? albumIndexSize.value : 0)
    + (artists?.enabled ? artistIndexSize.value : 0)
    + (genres?.enabled ? genreIndexSize.value : 0);
});
const relationIndexSizeValue = computed(() => {
  const take = relations.value;
  const tracks = getEntityByKey('tracks');
  const albums = getEntityByKey('albums');
  const artists = getEntityByKey('artists');
  const genres = getEntityByKey('genres');

  if (!tracks || !albums || !artists || !genres) return 0;

  const genreToArtist = relationIndexSize(genres.value, artists.value);
  const genreToAlbum = relationIndexSize(genres.value, albums.value);
  const genreToTrack = relationIndexSize(genres.value, tracks.value);
  const artistToAlbum = relationIndexSize(artists.value, albums.value);
  const albumToTracks = relationIndexSize(albums.value, tracks.value);
  const artistToTracks = relationIndexSize(artists.value, tracks.value);
  return genreToArtist * +take.genreToArtist
    + genreToAlbum * +take.genreToAlbum
    + genreToTrack * +take.genreToTrack
    + artistToAlbum * +take.artistToAlbum
    + albumToTracks * +take.albumToTracks
    + artistToTracks * +take.artistToTracks;
});
const totalSize = computed(() => relationIndexSizeValue.value + indexSize.value);
watch(unit, (nv, ov) => {
  const bytes = psramSize.value * (ov === 'B' ? 1 : ov === 'Kb' ? 1024 : 1024 * 1024);
  psramSize.value = convert(bytes);
  console.log('Unit changed', ov, nv, psramSize.value);
})
</script>
<template>
  <div class="form-container">
    <template v-for="entity in entities" :key="entity.key">
      <n-form-item :label="`Number of ${entity.label}`">
        <n-input-number v-model:value="entity.value" :placeholder="`Enter ${entity.label} number`" />
      </n-form-item>
      <n-form-item :label="`Average ${entity.label.slice(0, -1)} Length (chars)`">
        <n-input-number v-model:value="entity.averageLength"
          :placeholder="`Enter Average ${entity.label.slice(0, -1)} Length`" />
      </n-form-item>
    </template>
  </div>

  <n-divider></n-divider>

  <div class="enable-container">
    <h3>Enable/Disable Items</h3>
    <div class="checkbox-grid">
      <template v-for="entity in entities" :key="`enable-${entity.key}`">
        <label class="checkbox-item">
          <input type="checkbox" v-model="entity.enabled" />
          <span>{{ entity.label }}</span>
        </label>
      </template>
    </div>

    <h4>Relations</h4>
    <div class="checkbox-grid">
      <label class="checkbox-item">
        <input type="checkbox" v-model="relations.genreToArtist" />
        <span>Genre to Artist</span>
      </label>
      <label class="checkbox-item">
        <input type="checkbox" v-model="relations.genreToAlbum" />
        <span>Genre to Album</span>
      </label>
      <label class="checkbox-item">
        <input type="checkbox" v-model="relations.genreToTrack" />
        <span>Genre to Track</span>
      </label>
      <label class="checkbox-item">
        <input type="checkbox" v-model="relations.artistToAlbum" />
        <span>Artist to Album</span>
      </label>
      <label class="checkbox-item">
        <input type="checkbox" v-model="relations.albumToTracks" />
        <span>Album to Tracks</span>
      </label>
      <label class="checkbox-item">
        <input type="checkbox" v-model="relations.artistToTracks" />
        <span>Artist to Tracks</span>
      </label>
    </div>
  </div>
  <n-divider></n-divider>
  <n-form-item label="PSRAM size">
    <n-input-number v-model:value="psramSize" placeholder="Enter Average PSRAM size" />
  </n-form-item>

  <n-divider></n-divider>
  <div style="margin-bottom: 1em;">
    <n-radio-group v-model:value="unit" name="unit">
      <n-radio-button value="B">Bytes</n-radio-button>
      <n-radio-button value="Kb">Kilobytes</n-radio-button>
      <n-radio-button value="Mb">Megabytes</n-radio-button>
    </n-radio-group>
  </div>
  <div class="size-container">
    <span>trackIndexSize:</span><span>{{ convert(trackIndexSize) }} {{ unit }}</span>
    <span>albumIndexSize:</span><span>{{ convert(albumIndexSize) }} {{ unit }}</span>
    <span>artistIndexSize:</span><span>{{ convert(artistIndexSize) }} {{ unit }}</span>
    <span>genreIndexSize:</span><span>{{ convert(genreIndexSize) }} {{ unit }}</span>
    <span>relationIndexSize:</span><span>{{ convert(relationIndexSizeValue) }} {{ unit }}</span>
    <span></span><span></span>
    <span>Total:</span><span> {{ convert(totalSize) }} {{ unit }}</span>
    <span>PSRAM usage:</span><span> {{ Math.round(convert(totalSize) / psramSize * 100) }} %</span>
  </div>
</template>

<style scoped>
.form-container {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 8px;
  min-width: 100%;
}

.enable-container {
  margin: 1em 0;
}

.enable-container h3,
.enable-container h4 {
  margin: 1em 0 0.5em 0;
  font-size: 1.1em;
  font-weight: 600;
}

.checkbox-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 8px;
  margin-bottom: 1em;
}

.checkbox-item {
  display: flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  padding: 4px 8px;
  border-radius: 4px;
  transition: background-color 0.2s;
}

.checkbox-item:hover {
  background-color: rgba(0, 0, 0, 0.05);
}

.checkbox-item input[type="checkbox"] {
  margin: 0;
  cursor: pointer;
}

.checkbox-item span {
  user-select: none;
}

.size-container {
  display: grid;
  grid-template-columns: auto auto;
  column-gap: 20px;
  row-gap: 1em;
  min-width: 100%;
}
</style>