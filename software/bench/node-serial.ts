// Device over Node's serialport (the CLI runner).
import { SerialPort } from "serialport";
import { ReadlineParser } from "@serialport/parser-readline";
import { Device } from "./device.ts";

export function openNodeDevice(path: string, baudRate = 115200): Promise<Device> {
  return new Promise((resolve, reject) => {
    const port = new SerialPort({ path, baudRate }, (err) => {
      if (err) return reject(err);
      const parser = port.pipe(new ReadlineParser({ delimiter: "\n" }));
      // Ends whatever stray bytes opening the port left in the firmware's line.
      port.write("\n");
      resolve(
        new Device({
          write: (text) => void port.write(text),
          onLine: (cb) => void parser.on("data", cb),
          close: () => new Promise<void>((r) => port.close(() => r())),
        }),
      );
    });
  });
}
