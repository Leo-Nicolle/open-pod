<template>
  <div class="video-container">
    <video ref="videoRef" :src="withBase(src)" :controls="controls" :autoplay="autoplay" :loop="loop" :muted="muted"
      :poster="poster" :preload="preload" :width="width" :height="height" :class="videoClass" :style="videoStyle"
      @loadedmetadata="loadVideoMetadata">
      <slot>
        Your browser does not support the video tag.
      </slot>
    </video>
  </div>
</template>

<script setup>
import { withBase } from 'vitepress'
import { computed, ref, onMounted, nextTick } from 'vue'

const props = defineProps({
  src: {
    type: String,
    required: true
  },
  controls: {
    type: Boolean,
    default: true
  },
  autoplay: {
    type: Boolean,
    default: false
  },
  loop: {
    type: Boolean,
    default: false
  },
  muted: {
    type: Boolean,
    default: false
  },
  poster: {
    type: String,
    default: ''
  },
  preload: {
    type: String,
    default: 'metadata',
    validator: (value) => ['none', 'metadata', 'auto'].includes(value)
  },
  width: {
    type: [String, Number],
    default: '100%'
  },
  height: {
    type: [String, Number],
    default: 'auto'
  },
  responsive: {
    type: Boolean,
    default: true
  },
  aspectRatio: {
    type: String,
    default: 'auto' // Changed default to 'auto'
  },
  useOriginalAspectRatio: {
    type: Boolean,
    default: true // New prop to control automatic aspect ratio detection
  }
})

const videoRef = ref(null)
const originalAspectRatio = ref('16/9')
const isMetadataLoaded = ref(false)

const loadVideoMetadata = () => {
  if (!videoRef.value || !props.useOriginalAspectRatio) return

  const video = videoRef.value

  const handleLoadedMetadata = () => {
    if (video.videoWidth && video.videoHeight) {
      originalAspectRatio.value = `${video.videoWidth}/${video.videoHeight}`
      isMetadataLoaded.value = true
    }
  }

  if (video.readyState >= 1) {
    // Metadata already loaded
    handleLoadedMetadata()
  } else {
    // Wait for metadata to load
    video.addEventListener('loadedmetadata', handleLoadedMetadata, { once: true })
  }
}

onMounted(async () => {
  await nextTick()
  loadVideoMetadata()
})

const computedAspectRatio = computed(() => {
  if (props.aspectRatio !== 'auto') {
    return props.aspectRatio
  }
  const res = props.useOriginalAspectRatio && isMetadataLoaded.value
    ? originalAspectRatio.value
    : '16/9'
  return res
})

const videoClass = computed(() => ({
  'video-responsive': props.responsive,
  'video-fixed': !props.responsive
}))

const videoStyle = computed(() => {
  if (!props.responsive) {
    return {
      width: typeof props.width === 'number' ? `${props.width}px` : props.width,
      height: typeof props.height === 'number' ? `${props.height}px` : props.height
    }
  }
  return {}
})
</script>

<style scoped>
.video-container {
  margin: 1rem 0;
}

.video-responsive {
  width: 100%;
  height: auto;
  max-width: 100%;
}

.video-fixed {
  display: block;
}

/* Responsive aspect ratio container */
.video-container:has(.video-responsive) {
  position: relative;
  width: 100%;
  aspect-ratio: v-bind(computedAspectRatio);
}

.video-container:has(.video-responsive) .video-responsive {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
}

/* Fallback for browsers that don't support :has() */
@supports not selector(:has(*)) {
  .video-responsive {
    aspect-ratio: v-bind(computedAspectRatio);
  }
}
</style>