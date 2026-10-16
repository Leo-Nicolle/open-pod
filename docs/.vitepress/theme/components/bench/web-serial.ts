// The bench Device over the browser's Web Serial API (Chrome/Edge).
import { Device, type Transport } from "@bench/device.ts";

// Web Serial isn't in TypeScript's DOM lib yet.
type WebSerialPort = {
  open(opts: { baudRate: number }): Promise<void>;
  close(): Promise<void>;
  readable: ReadableStream<Uint8Array>;
  writable: WritableStream<Uint8Array>;
  getInfo(): { usbVendorId?: number; usbProductId?: number };
};

export const webSerialSupported = () => typeof navigator !== "undefined" && "serial" in navigator;

/** ST-Link's USB vendor id: the Nucleo's virtual COM port. */
const STLINK_VENDOR = 0x0483;

/** Asks the user to pick the Nucleo's port, opens it and wraps it in a Device. */
/** Traffic counters, for diagnosing a link that opens but doesn't talk. */
export type LinkStats = { bytesIn: number; linesIn: number; bytesOut: number; writeError: string; lastLine: string };

export async function connectWebSerial(
  onClosed: (error?: unknown) => void,
): Promise<{ device: Device; label: string; stats: LinkStats }> {
  const stats: LinkStats = { bytesIn: 0, linesIn: 0, bytesOut: 0, writeError: "", lastLine: "" };
  const serial = (navigator as any).serial;
  // An ST-Link this site was already allowed to use: no picker. Otherwise one
  // picker with every port (a second requestPort() after a cancelled one
  // isn't allowed, and filtering on the ST-Link would hide other adapters).
  const granted: WebSerialPort[] = await serial.getPorts();
  const port: WebSerialPort =
    granted.find((p) => p.getInfo().usbVendorId === STLINK_VENDOR) ?? (await serial.requestPort());
  await port.open({ baudRate: 115200 });

  const listeners: ((line: string) => void)[] = [];
  const reader = port.readable.getReader();
  const writer = port.writable.getWriter();
  const decoder = new TextDecoder();
  const encoder = new TextEncoder();
  let closing = false;

  const pump = (async () => {
    let buffer = "";
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        stats.bytesIn += value.length;
        buffer += decoder.decode(value, { stream: true });
        let nl: number;
        while ((nl = buffer.indexOf("\n")) >= 0) {
          const line = buffer.slice(0, nl);
          buffer = buffer.slice(nl + 1);
          stats.linesIn++;
          stats.lastLine = line.slice(0, 120);
          for (const l of listeners) l(line);
        }
      }
      if (!closing) onClosed();
    } catch (e) {
      if (closing) return;
      if (e instanceof DOMException && e.name === "NetworkError" && stats.bytesIn === 0) {
        // Not unplugged: Chrome < 156 on Linux reads the tty's VMIN as the
        // last program left it, and pyserial (PlatformIO's monitor) leaves
        // 0, so an idle read returns nothing and Chrome calls it a lost
        // device (Chromium issue 559872992).
        onClosed(
          new Error(
            `Chrome lost the port before reading anything. On Linux that's usually the tty left with VMIN=0 ` +
              `by PlatformIO's monitor or another pyserial tool. Fix it with: stty -F /dev/ttyACM0 min 1 time 0, ` +
              `then connect again.`,
          ),
        );
      } else {
        // Unplugged, or the board reset its USB.
        onClosed(e);
      }
    }
  })();

  const transport: Transport = {
    write: (text) => {
      const bytes = encoder.encode(text);
      writer.write(bytes).then(
        () => (stats.bytesOut += bytes.length),
        (e) => (stats.writeError = String(e)),
      );
    },
    onLine: (cb) => void listeners.push(cb),
    close: async () => {
      closing = true;
      await reader.cancel().catch(() => {});
      await pump;
      reader.releaseLock();
      writer.releaseLock();
      await port.close().catch(() => {});
    },
  };
  // Ends whatever stray bytes opening the port left in the firmware's line.
  transport.write("\n");
  const info = port.getInfo();
  const label = info.usbVendorId === STLINK_VENDOR ? "Nucleo (ST-Link)" : "Serial port";
  return { device: new Device(transport), label, stats };
}
