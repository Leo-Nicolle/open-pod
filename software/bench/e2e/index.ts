// Every e2e test, in the order they run.
import { buttonTests } from "./buttons.spec.ts";
import { scrollTests } from "./scroll.spec.ts";
import { seekTests } from "./seek.spec.ts";
import type { E2ETest } from "./e2e.ts";

export * from "./e2e.ts";

export const TESTS: E2ETest[] = [...buttonTests, ...scrollTests, ...seekTests];

/** Tests whose name or group contains `filter` (case-insensitive); all without one. */
export function selectTests(filter?: string): E2ETest[] {
  if (!filter) return TESTS;
  const f = filter.toLowerCase();
  return TESTS.filter((t) => t.name.toLowerCase().includes(f) || t.group.toLowerCase().includes(f));
}
