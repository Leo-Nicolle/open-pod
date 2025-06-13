#!/usr/bin/env node

const GREEN = "\x1b[32m";
const RED = "\x1b[31m";
const YELLOW = "\x1b[33m";
const GRAY = "\x1b[90m";
const RESET = "\x1b[0m";
const BOLD = "\x1b[1m";

function parseBlock(blockLines) {
  // Remove empty lines
  const lines = blockLines.filter((l) => l.trim().length > 0);
  if (lines.length === 0) return null;

  // Find header line: <file>:<line>:
  const headerIdx = lines.findIndex((l) => l.match(/^[^:]+:\d+:/));
  if (headerIdx === -1) return null;

  const headerLine = lines[headerIdx];
  const headerMatch = headerLine.match(/^(.+):(\d+):/);
  if (!headerMatch) return null;

  const file = headerMatch[1].trim();
  const line = headerMatch[2].trim();

  // Find test case name
  let name = null;
  let subcase = null;
  for (let i = headerIdx + 1; i < lines.length; ++i) {
    const l = lines[i];
    const m = l.match(/^TEST CASE:\s+(.*)$/);
    if (m) {
      name = m[1].trim();
      continue;
    }
    const s = l.match(/^\s{2,}(.+)$/);
    if (
      s &&
      !l.includes("ERROR:") &&
      !l.includes("MESSAGE:") &&
      !l.includes("values:")
    ) {
      subcase = s[1].trim();
      continue;
    }
  }

  // Find error line(s)
  let status = "pass";
  let errorDetails = [];
  for (const l of lines) {
    if (
      l.includes("ERROR:") ||
      l.includes("FATAL ERROR") ||
      l.includes("CRASHED") ||
      l.includes("is NOT correct!") ||
      l.includes("SIGABRT") ||
      l.toLowerCase().includes("signal")
    ) {
      status = "fail";
      errorDetails.push(l);
    }
    if (l.includes("values:")) {
      errorDetails.push(l);
    }
  }

  return {
    file,
    line,
    name,
    subcase,
    status,
    errorDetails,
  };
}

function groupByTest(tests) {
  // Returns: { [file]: { [testName]: [subtests] } }
  const grouped = {};
  for (const t of tests) {
    if (!t.name) continue;
    if (!grouped[t.file]) grouped[t.file] = {};
    if (!grouped[t.file][t.name]) grouped[t.file][t.name] = [];
    grouped[t.file][t.name].push(t);
  }
  return grouped;
}

function main() {
  const readline = require("readline");
  const rl = readline.createInterface({
    input: process.stdin,
    crlfDelay: Infinity,
  });

  let currentBlock = [];
  const testResults = [];

  rl.on("line", (line) => {
    if (line.match(/^=+$/)) {
      // End of block
      const result = parseBlock(currentBlock);
      if (result) testResults.push(result);
      currentBlock = [];
    } else {
      currentBlock.push(line);
    }
  });

  rl.on("close", () => {
    // Handle last block if any
    if (currentBlock.length > 0) {
      const result = parseBlock(currentBlock);
      if (result) testResults.push(result);
    }

    // Group by test and subtest
    const grouped = groupByTest(testResults);

    // Print summary
    for (const [file, testsByName] of Object.entries(grouped)) {
      // Count total and failed subtests
      let total = 0,
        failed = 0;
      for (const subtests of Object.values(testsByName)) {
        total += subtests.length;
        failed += subtests.filter((t) => t.status === "fail").length;
      }
      if (failed > 0) {
        console.log(` ${RED}❯${RESET} ${file} (${total})`);
      } else {
        console.log(` ${GREEN}✓${RESET} ${file} (${total})`);
      }
      for (const [testName, subtests] of Object.entries(testsByName)) {
        // If there is only one subtest and no subcase, print test name directly
        if (
          subtests.length === 1 &&
          (!subtests[0].subcase || subtests[0].subcase === "")
        ) {
          const t = subtests[0];
          if (t.status === "fail") {
            console.log(` ${RED}× ${testName}${RESET}`);
          } else {
            console.log(` ${GREEN}✓ ${testName}${RESET}`);
          }
        } else {
          // Print test name, then each subcase indented
          console.log(` ${BOLD}${testName}${RESET}`);
          for (const t of subtests) {
            let label = t.subcase ? t.subcase : "(no subcase)";
            if (t.status === "fail") {
              if (t.subcase) {
                console.log(`   ${RED}× ${label}${RESET}`);
              } else {
                // No subcase, print test name in red
                console.log(`   ${RED}× ${testName}${RESET}`);
              }
            } else {
              if (t.subcase) {
                console.log(`   ${GREEN}✓ ${label}${RESET}`);
              } else {
                // No subcase, print test name in green
                console.log(`   ${GREEN}✓ ${testName}${RESET}`);
              }
            }
          }
        }
      }
      console.log("");
    }

    // Print failures
    const failedTests = testResults.filter((t) => t.status === "fail");
    if (failedTests.length > 0) {
      const separator = "⎯".repeat(80);
      console.log(
        `${separator} ${RED}Failed Tests ${failedTests.length}${RESET} ${separator}`
      );
      console.log("");
      for (const t of failedTests) {
        let label = t.name;
        if (t.subcase) label += ` → ${t.subcase}`;
        console.log(` ${RED}FAIL${RESET}  ${t.file} > ${label}`);
        for (const detail of t.errorDetails) {
          if (detail.includes("ERROR:")) {
            const msg = detail.split("ERROR:").pop().trim();
            console.log(`${RED}${msg}${RESET}`);
          } else if (detail.includes("is NOT correct!")) {
            console.log(`${RED}Assertion failed${RESET}`);
          } else if (detail.includes("values:")) {
            console.log(`${GRAY}${detail}${RESET}`);
          } else if (
            detail.includes("FATAL ERROR") ||
            detail.includes("CRASHED")
          ) {
            console.log(`${RED}${detail}${RESET}`);
          }
        }
        console.log(` ${GRAY}❯ ${t.file}:${t.line}${RESET}`);
        console.log("");
      }
    }
  });
}

if (require.main === module) {
  main();
}
