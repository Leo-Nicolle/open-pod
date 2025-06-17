class FuzzyRadixTree {
    constructor() {
        this.root = new RadixNode('', null, 0);
        this.size = 0;
        
        // Fuzzy search configuration
        this.config = {
            maxDistance: 2,           // Maximum Levenshtein distance
            prefixBoost: 0.3,        // Boost for prefix matches
            phonetic: true,          // Enable phonetic matching
            caseSensitive: false,    // Case sensitivity
            minQueryLength: 2,       // Minimum query length for fuzzy search
            maxResults: 20           // Maximum results to return
        };
        
        // Precompute phonetic codes for common patterns
        this.phoneticMap = this._buildPhoneticMap();
        
        // Cache for fuzzy search results
        this.searchCache = new Map();
        this.cacheSize = 1000;
    }
    
    // Radix tree node with fuzzy search optimizations
    class RadixNode {
        constructor(key, value, depth) {
            this.key = key;                    // String key for this node
            this.value = value;                // Value if this is a leaf
            this.children = new Map();         // Children keyed by first character
            this.isLeaf = value !== null;      // Is this a terminal node
            this.depth = depth;                // Depth in tree (for optimization)
            
            // Fuzzy search optimizations
            this.phonetic = null;              // Phonetic code cache
            this.ngrams = null;                // N-gram cache for similarity
            this.frequency = 0;                // Usage frequency for ranking
        }
        
        // Get or compute phonetic code
        getPhonetic() {
            if (this.phonetic === null && this.isLeaf) {
                this.phonetic = this._parent._computePhonetic(this.getFullKey());
            }
            return this.phonetic;
        }
        
        // Get full key path from root
        getFullKey() {
            let fullKey = '';
            let current = this;
            let path = [];
            
            while (current && current.key !== '') {
                path.unshift(current.key);
                current = current.parent;
            }
            
            return path.join('');
        }
        
        // Update frequency for ranking
        incrementFrequency() {
            this.frequency++;
        }
    }
    
    // Build phonetic mapping for common substitutions
    _buildPhoneticMap() {
        return {
            // Soundex-like mappings
            'b': 'p', 'p': 'b',
            'c': 'k', 'k': 'c', 'q': 'k',
            'd': 't', 't': 'd',
            'f': 'v', 'v': 'f',
            'g': 'j', 'j': 'g',
            's': 'z', 'z': 's',
            
            // Common typos
            'i': 'e', 'e': 'i',
            'a': 'e', 'o': 'u',
            
            // Double letters
            'll': 'l', 'ss': 's', 'nn': 'n', 'mm': 'm',
            'tt': 't', 'pp': 'p', 'cc': 'c'
        };
    }
    
    // Compute phonetic code for a string
    _computePhonetic(str) {
        if (!str) return '';
        
        str = str.toLowerCase().replace(/[^a-z]/g, '');
        
        // Remove consecutive duplicates
        str = str.replace(/(.)\1+/g, '$1');
        
        // Apply phonetic substitutions
        for (let [from, to] of Object.entries(this.phoneticMap)) {
            str = str.replace(new RegExp(from, 'g'), to);
        }
        
        return str;
    }
    
    // Insert a key-value pair into the tree
    insert(key, value) {
        if (!key) return false;
        
        const normalizedKey = this.config.caseSensitive ? key : key.toLowerCase();
        this._insertRecursive(this.root, normalizedKey, value, 0);
        this.size++;
        
        // Clear search cache as tree structure changed
        this.searchCache.clear();
        
        return true;
    }
    
    _insertRecursive(node, key, value, depth) {
        if (key.length === 0) {
            node.value = value;
            node.isLeaf = true;
            return node;
        }
        
        const firstChar = key[0];
        let child = node.children.get(firstChar);
        
        if (!child) {
            // Create new child with full remaining key
            child = new RadixNode(key, value, depth + 1);
            child.parent = node;
            child.isLeaf = true;
            node.children.set(firstChar, child);
            return child;
        }
        
        // Find common prefix between child key and remaining key
        const commonLength = this._findCommonPrefix(child.key, key);
        
        if (commonLength === child.key.length) {
            // Child key is prefix of our key, continue down
            const remainingKey = key.slice(commonLength);
            return this._insertRecursive(child, remainingKey, value, depth + 1);
        }
        
        if (commonLength === key.length) {
            // Our key is prefix of child key, need to split
            const oldKey = child.key;
            const oldValue = child.value;
            const oldChildren = child.children;
            const oldIsLeaf = child.isLeaf;
            
            // Update child to be our node
            child.key = key;
            child.value = value;
            child.isLeaf = true;
            child.children = new Map();
            
            // Create new child for old data
            const newChild = new RadixNode(
                oldKey.slice(commonLength), 
                oldValue, 
                depth + 2
            );
            newChild.parent = child;
            newChild.children = oldChildren;
            newChild.isLeaf = oldIsLeaf;
            
            // Update parent pointers for grandchildren
            for (let grandchild of newChild.children.values()) {
                grandchild.parent = newChild;
            }
            
            child.children.set(newChild.key[0], newChild);
            return child;
        }
        
        // Keys diverge, need to split child
        const oldKey = child.key;
        const oldValue = child.value;
        const oldChildren = child.children;
        const oldIsLeaf = child.isLeaf;
        
        // Update child to be the common prefix
        child.key = child.key.slice(0, commonLength);
        child.value = null;
        child.isLeaf = false;
        child.children = new Map();
        
        // Create child for old data
        const oldChild = new RadixNode(
            oldKey.slice(commonLength),
            oldValue,
            depth + 2
        );
        oldChild.parent = child;
        oldChild.children = oldChildren;
        oldChild.isLeaf = oldIsLeaf;
        
        // Update parent pointers
        for (let grandchild of oldChild.children.values()) {
            grandchild.parent = oldChild;
        }
        
        // Create child for new data
        const newChild = new RadixNode(
            key.slice(commonLength),
            value,
            depth + 2
        );
        newChild.parent = child;
        newChild.isLeaf = true;
        
        child.children.set(oldChild.key[0], oldChild);
        child.children.set(newChild.key[0], newChild);
        
        return child;
    }
    
    // Find longest common prefix between two strings
    _findCommonPrefix(str1, str2) {
        let i = 0;
        while (i < str1.length && i < str2.length && str1[i] === str2[i]) {
            i++;
        }
        return i;
    }
    
    // Exact search
    search(key) {
        if (!key) return null;
        
        const normalizedKey = this.config.caseSensitive ? key : key.toLowerCase();
        const node = this._searchExact(this.root, normalizedKey);
        
        if (node && node.isLeaf) {
            node.incrementFrequency();
            return node.value;
        }
        
        return null;
    }
    
    _searchExact(node, key) {
        if (key.length === 0) {
            return node;
        }
        
        const firstChar = key[0];
        const child = node.children.get(firstChar);
        
        if (!child) {
            return null;
        }
        
        const commonLength = this._findCommonPrefix(child.key, key);
        
        if (commonLength < child.key.length) {
            return null; // Key doesn't match
        }
        
        if (commonLength === key.length) {
            return child; // Found exact match
        }
        
        // Continue searching
        return this._searchExact(child, key.slice(commonLength));
    }
    
    // Fuzzy search with multiple strategies
    fuzzySearch(query, options = {}) {
        if (!query || query.length < this.config.minQueryLength) {
            return [];
        }
        
        const opts = { ...this.config, ...options };
        const normalizedQuery = opts.caseSensitive ? query : query.toLowerCase();
        
        // Check cache first
        const cacheKey = `${normalizedQuery}-${JSON.stringify(opts)}`;
        if (this.searchCache.has(cacheKey)) {
            return this.searchCache.get(cacheKey);
        }
        
        const results = new Map(); // Use Map to avoid duplicates
        
        // Strategy 1: Prefix search (fastest, highest score)
        this._prefixSearch(normalizedQuery, results, opts);
        
        // Strategy 2: Levenshtein distance search
        this._levenshteinSearch(normalizedQuery, results, opts);
        
        // Strategy 3: Phonetic search (if enabled)
        if (opts.phonetic) {
            this._phoneticSearch(normalizedQuery, results, opts);
        }
        
        // Strategy 4: N-gram similarity search
        this._ngramSearch(normalizedQuery, results, opts);
        
        // Convert to array and sort by score
        const sortedResults = Array.from(results.values())
            .sort((a, b) => b.score - a.score)
            .slice(0, opts.maxResults);
        
        // Cache results
        if (this.searchCache.size >= this.cacheSize) {
            // Clear oldest entries
            const entries = Array.from(this.searchCache.entries());
            for (let i = 0; i < Math.floor(this.cacheSize / 2); i++) {
                this.searchCache.delete(entries[i][0]);
            }
        }
        this.searchCache.set(cacheKey, sortedResults);
        
        return sortedResults;
    }
    
    // Prefix search for exact prefix matches
    _prefixSearch(query, results, opts) {
        const matches = this._collectWithPrefix(this.root, query, opts.maxResults);
        
        for (let match of matches) {
            const key = match.getFullKey();
            const score = this._calculatePrefixScore(key, query, match.frequency);
            
            if (!results.has(match.value) || results.get(match.value).score < score) {
                results.set(match.value, {
                    value: match.value,
                    key: key,
                    score: score,
                    matchType: 'prefix'
                });
            }
        }
    }
    
    // Collect all nodes with given prefix
    _collectWithPrefix(node, prefix, maxResults) {
        const results = [];
        this._collectWithPrefixRecursive(node, prefix, results, maxResults);
        return results;
    }
    
    _collectWithPrefixRecursive(node, prefix, results, maxResults) {
        if (results.length >= maxResults) return;
        
        if (prefix.length === 0) {
            // Collect all descendants
            this._collectAllDescendants(node, results, maxResults);
            return;
        }
        
        const firstChar = prefix[0];
        const child = node.children.get(firstChar);
        
        if (!child) return;
        
        const commonLength = this._findCommonPrefix(child.key, prefix);
        
        if (commonLength === child.key.length) {
            // Child key is prefix of our search
            const remainingPrefix = prefix.slice(commonLength);
            this._collectWithPrefixRecursive(child, remainingPrefix, results, maxResults);
        } else if (commonLength === prefix.length) {
            // Our prefix matches part of child key
            this._collectAllDescendants(child, results, maxResults);
        }
    }
    
    // Collect all leaf descendants of a node
    _collectAllDescendants(node, results, maxResults) {
        if (results.length >= maxResults) return;
        
        if (node.isLeaf) {
            results.push(node);
        }
        
        for (let child of node.children.values()) {
            this._collectAllDescendants(child, results, maxResults);
        }
    }
    
    // Levenshtein distance search
    _levenshteinSearch(query, results, opts) {
        this._levenshteinSearchRecursive(this.root, '', query, 0, results, opts);
    }
    
    _levenshteinSearchRecursive(node, currentKey, query, currentDistance, results, opts) {
        if (currentDistance > opts.maxDistance) return;
        
        if (node.isLeaf) {
            const fullKey = currentKey;
            const distance = this._levenshteinDistance(fullKey, query);
            
            if (distance <= opts.maxDistance) {
                const score = this._calculateDistanceScore(fullKey, query, distance, node.frequency);
                
                if (!results.has(node.value) || results.get(node.value).score < score) {
                    results.set(node.value, {
                        value: node.value,
                        key: fullKey,
                        score: score,
                        matchType: 'levenshtein',
                        distance: distance
                    });
                }
            }
        }
        
        // Continue to children
        for (let child of node.children.values()) {
            const newKey = currentKey + child.key;
            
            // Prune search if minimum possible distance exceeds threshold
            const minDistance = Math.abs(newKey.length - query.length);
            if (minDistance <= opts.maxDistance) {
                this._levenshteinSearchRecursive(child, newKey, query, currentDistance, results, opts);
            }
        }
    }
    
    // Compute Levenshtein distance between two strings
    _levenshteinDistance(str1, str2) {
        const m = str1.length;
        const n = str2.length;
        
        if (m === 0) return n;
        if (n === 0) return m;
        
        // Use single array optimization
        let prev = new Array(n + 1);
        let curr = new Array(n + 1);
        
        // Initialize first row
        for (let j = 0; j <= n; j++) {
            prev[j] = j;
        }
        
        for (let i = 1; i <= m; i++) {
            curr[0] = i;
            
            for (let j = 1; j <= n; j++) {
                if (str1[i - 1] === str2[j - 1]) {
                    curr[j] = prev[j - 1];
                } else {
                    curr[j] = 1 + Math.min(
                        prev[j],     // deletion
                        curr[j - 1], // insertion
                        prev[j - 1]  // substitution
                    );
                }
            }
            
            // Swap arrays
            [prev, curr] = [curr, prev];
        }
        
        return prev[n];
    }
    
    // Phonetic search for sound-alike matches
    _phoneticSearch(query, results, opts) {
        const queryPhonetic = this._computePhonetic(query);
        if (!queryPhonetic) return;
        
        this._phoneticSearchRecursive(this.root, '', queryPhonetic, results, opts);
    }
    
    _phoneticSearchRecursive(node, currentKey, queryPhonetic, results, opts) {
        if (node.isLeaf) {
            const fullKey = currentKey;
            const keyPhonetic = this._computePhonetic(fullKey);
            
            if (keyPhonetic && keyPhonetic === queryPhonetic) {
                const score = this._calculatePhoneticScore(fullKey, node.frequency);
                
                if (!results.has(node.value) || results.get(node.value).score < score) {
                    results.set(node.value, {
                        value: node.value,
                        key: fullKey,
                        score: score,
                        matchType: 'phonetic'
                    });
                }
            }
        }
        
        for (let child of node.children.values()) {
            this._phoneticSearchRecursive(child, currentKey + child.key, queryPhonetic, results, opts);
        }
    }
    
    // N-gram similarity search
    _ngramSearch(query, results, opts) {
        const queryNgrams = this._generateNgrams(query, 2);
        if (queryNgrams.length === 0) return;
        
        this._ngramSearchRecursive(this.root, '', queryNgrams, results, opts);
    }
    
    _ngramSearchRecursive(node, currentKey, queryNgrams, results, opts) {
        if (node.isLeaf) {
            const fullKey = currentKey;
            const keyNgrams = this._generateNgrams(fullKey, 2);
            const similarity = this._calculateNgramSimilarity(queryNgrams, keyNgrams);
            
            if (similarity > 0.3) { // Minimum similarity threshold
                const score = this._calculateNgramScore(fullKey, similarity, node.frequency);
                
                if (!results.has(node.value) || results.get(node.value).score < score) {
                    results.set(node.value, {
                        value: node.value,
                        key: fullKey,
                        score: score,
                        matchType: 'ngram',
                        similarity: similarity
                    });
                }
            }
        }
        
        for (let child of node.children.values()) {
            this._ngramSearchRecursive(child, currentKey + child.key, queryNgrams, results, opts);
        }
    }
    
    // Generate n-grams for a string
    _generateNgrams(str, n) {
        if (str.length < n) return [str];
        
        const ngrams = [];
        for (let i = 0; i <= str.length - n; i++) {
            ngrams.push(str.slice(i, i + n));
        }
        return ngrams;
    }
    
    // Calculate n-gram similarity using Jaccard coefficient
    _calculateNgramSimilarity(ngrams1, ngrams2) {
        const set1 = new Set(ngrams1);
        const set2 = new Set(ngrams2);
        
        const intersection = new Set([...set1].filter(x => set2.has(x)));
        const union = new Set([...set1, ...set2]);
        
        return intersection.size / union.size;
    }
    
    // Scoring functions
    _calculatePrefixScore(key, query, frequency) {
        const prefixLength = this._findCommonPrefix(key, query);
        const prefixRatio = prefixLength / query.length;
        const lengthPenalty = Math.abs(key.length - query.length) / Math.max(key.length, query.length);
        const frequencyBoost = Math.log(frequency + 1) * 0.1;
        
        return (prefixRatio * 2 + this.config.prefixBoost - lengthPenalty + frequencyBoost);
    }
    
    _calculateDistanceScore(key, query, distance, frequency) {
        const maxLen = Math.max(key.length, query.length);
        const similarity = 1 - (distance / maxLen);
        const frequencyBoost = Math.log(frequency + 1) * 0.1;
        
        return similarity + frequencyBoost;
    }
    
    _calculatePhoneticScore(key, frequency) {
        const frequencyBoost = Math.log(frequency + 1) * 0.1;
        return 0.7 + frequencyBoost; // Lower than exact matches but higher than poor fuzzy matches
    }
    
    _calculateNgramScore(key, similarity, frequency) {
        const frequencyBoost = Math.log(frequency + 1) * 0.1;
        return similarity * 0.8 + frequencyBoost;
    }
    
    // Get all entries (for debugging)
    getAllEntries() {
        const entries = [];
        this._getAllEntriesRecursive(this.root, '', entries);
        return entries.sort((a, b) => a.key.localeCompare(b.key));
    }
    
    _getAllEntriesRecursive(node, currentKey, entries) {
        if (node.isLeaf) {
            entries.push({
                key: currentKey,
                value: node.value,
                frequency: node.frequency
            });
        }
        
        for (let child of node.children.values()) {
            this._getAllEntriesRecursive(child, currentKey + child.key, entries);
        }
    }
    
    // Statistics
    getStats() {
        return {
            size: this.size,
            cacheSize: this.searchCache.size,
            config: { ...this.config }
        };
    }
    
    // Clear search cache
    clearCache() {
        this.searchCache.clear();
    }
    
    // Update configuration
    updateConfig(newConfig) {
        this.config = { ...this.config, ...newConfig };
        this.clearCache(); // Clear cache as search behavior may change
    }
}

// Example usage and testing
function testFuzzyRadixTree() {
    const tree = new FuzzyRadixTree();
    
    // Add some music data
    const musicData = [
        'The Beatles',
        'The Rolling Stones',
        'Led Zeppelin',
        'Pink Floyd',
        'Queen',
        'The Who',
        'Black Sabbath',
        'Deep Purple',
        'The Doors',
        'Jimi Hendrix',
        'Bob Dylan',
        'The Beach Boys',
        'The Kinks',
        'The Velvet Underground',
        'David Bowie'
    ];
    
    // Insert data
    musicData.forEach((artist, index) => {
        tree.insert(artist, { id: index, name: artist, type: 'artist' });
    });
    
    console.log('Tree Stats:', tree.getStats());
    
    // Test different types of searches
    console.log('\\n=== Fuzzy Search Tests ===');
    
    // Test 1: Exact match
    console.log('Exact "Queen":', tree.search('Queen'));
    
    // Test 2: Prefix search
    console.log('Fuzzy "The":', tree.fuzzySearch('The'));
    
    // Test 3: Typos
    console.log('Fuzzy "Beatls" (typo):', tree.fuzzySearch('Beatls'));
    
    // Test 4: Phonetic
    console.log('Fuzzy "Qween" (phonetic):', tree.fuzzySearch('Qween'));
    
    // Test 5: Partial match
    console.log('Fuzzy "Zeppelin":', tree.fuzzySearch('Zeppelin'));
    
    // Test 6: Multiple strategies
    console.log('Fuzzy "Stones":', tree.fuzzySearch('Stones'));
    
    return tree;
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = FuzzyRadixTree;
}

// Auto-run test if in browser console
if (typeof window !== 'undefined') {
    window.FuzzyRadixTree = FuzzyRadixTree;
    window.testFuzzyRadixTree = testFuzzyRadixTree;
}