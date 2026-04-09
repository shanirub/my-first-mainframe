# PoC Sequence Diagrams

Three diagrams cover the full PoC flow. Read them in order.

---

## Diagram 1 — Boot sequence

Both MCUs go through identical boot steps. `SharedBus::init()` must complete
before any task is created, because the ISR is registered inside `init()` and
the `rxSemaphore` it signals must already exist. `vTaskStartScheduler()` hands
control to FreeRTOS — `loop()` is never called.

<svg width="100%" viewBox="0 0 680 420" xmlns="http://www.w3.org/2000/svg">
<defs>
  <marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
    <path d="M2 1L8 5L2 9" fill="none" stroke="context-stroke" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  </marker>
</defs>
<rect x="40"  y="50" width="270" height="36" rx="8" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="14" font-weight="500" x="175" y="68" text-anchor="middle" dominant-baseline="central" fill="#3C3489">MCU #1 — master console</text>
<rect x="370" y="50" width="270" height="36" rx="8" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="14" font-weight="500" x="505" y="68" text-anchor="middle" dominant-baseline="central" fill="#085041">MCU #2 — transaction processor</text>
<line x1="175" y1="86" x2="175" y2="400" stroke="#888780" stroke-width="0.5" stroke-dasharray="4 3"/>
<line x1="505" y1="86" x2="505" y2="400" stroke="#888780" stroke-width="0.5" stroke-dasharray="4 3"/>
<text font-family="sans-serif" font-size="12" x="175" y="106" text-anchor="middle" fill="#5F5E5A">setup()</text>
<text font-family="sans-serif" font-size="12" x="505" y="106" text-anchor="middle" fill="#5F5E5A">setup()</text>
<rect x="145" y="118" width="60" height="28" rx="4" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="175" y="132" text-anchor="middle" dominant-baseline="central" fill="#2C2C2A">SharedBus.init()</text>
<text font-family="sans-serif" font-size="12" x="175" y="160" text-anchor="middle" fill="#5F5E5A">creates mutex</text>
<text font-family="sans-serif" font-size="12" x="175" y="178" text-anchor="middle" fill="#5F5E5A">creates rxSemaphore</text>
<text font-family="sans-serif" font-size="12" x="175" y="196" text-anchor="middle" fill="#5F5E5A">beginSlave(0x08)</text>
<text font-family="sans-serif" font-size="12" x="175" y="214" text-anchor="middle" fill="#5F5E5A">registers ISR</text>
<rect x="475" y="118" width="60" height="28" rx="4" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="505" y="132" text-anchor="middle" dominant-baseline="central" fill="#2C2C2A">SharedBus.init()</text>
<text font-family="sans-serif" font-size="12" x="505" y="160" text-anchor="middle" fill="#5F5E5A">creates mutex</text>
<text font-family="sans-serif" font-size="12" x="505" y="178" text-anchor="middle" fill="#5F5E5A">creates rxSemaphore</text>
<text font-family="sans-serif" font-size="12" x="505" y="196" text-anchor="middle" fill="#5F5E5A">beginSlave(0x09)</text>
<text font-family="sans-serif" font-size="12" x="505" y="214" text-anchor="middle" fill="#5F5E5A">registers ISR</text>
<line x1="40" y1="228" x2="640" y2="228" stroke="#D3D1C7" stroke-width="0.5" stroke-dasharray="3 3"/>
<text font-family="sans-serif" font-size="12" x="175" y="246" text-anchor="middle" fill="#5F5E5A">xTaskCreate ×4</text>
<rect x="60"  y="258" width="100" height="26" rx="5" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="110" y="271" text-anchor="middle" dominant-baseline="central" fill="#3C3489">Sender A  pri=2</text>
<rect x="170" y="258" width="100" height="26" rx="5" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="220" y="271" text-anchor="middle" dominant-baseline="central" fill="#3C3489">Sender B  pri=2</text>
<rect x="60"  y="292" width="100" height="26" rx="5" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="110" y="305" text-anchor="middle" dominant-baseline="central" fill="#3C3489">Receiver  pri=3</text>
<rect x="170" y="292" width="100" height="26" rx="5" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="220" y="305" text-anchor="middle" dominant-baseline="central" fill="#3C3489">OLED      pri=1</text>
<text font-family="sans-serif" font-size="12" x="505" y="246" text-anchor="middle" fill="#5F5E5A">xTaskCreate ×3</text>
<rect x="390" y="258" width="100" height="26" rx="5" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="440" y="271" text-anchor="middle" dominant-baseline="central" fill="#085041">Receiver  pri=3</text>
<rect x="500" y="258" width="100" height="26" rx="5" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="550" y="271" text-anchor="middle" dominant-baseline="central" fill="#085041">Logic     pri=2</text>
<rect x="390" y="292" width="100" height="26" rx="5" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="440" y="305" text-anchor="middle" dominant-baseline="central" fill="#085041">OLED      pri=1</text>
<text font-family="sans-serif" font-size="12" x="175" y="338" text-anchor="middle" fill="#5F5E5A">vTaskStartScheduler()</text>
<line x1="175" y1="346" x2="175" y2="362" stroke="#5F5E5A" stroke-width="1" marker-end="url(#arrow)"/>
<text font-family="sans-serif" font-size="12" x="175" y="376" text-anchor="middle" fill="#5F5E5A">FreeRTOS takes over — loop() never called</text>
<text font-family="sans-serif" font-size="12" x="505" y="338" text-anchor="middle" fill="#5F5E5A">vTaskStartScheduler()</text>
<line x1="505" y1="346" x2="505" y2="362" stroke="#5F5E5A" stroke-width="1" marker-end="url(#arrow)"/>
<text font-family="sans-serif" font-size="12" x="505" y="376" text-anchor="middle" fill="#5F5E5A">FreeRTOS takes over — loop() never called</text>
</svg>

---

## Diagram 2 — Send / receive flow

The core of the PoC. Sender A and Sender B on MCU #1 run at different
intervals (2s and 3s) so they occasionally try to send simultaneously — this
is what stress-tests the mutex. The blocked task (dashed box) waits until the
mutex is released before proceeding. Every I2C transmission is a thick arrow
crossing the MCU boundary. Internal task handoffs via queue are thin arrows
staying within one MCU's column group.

<svg width="100%" viewBox="0 0 680 1020" xmlns="http://www.w3.org/2000/svg">
<defs>
  <marker id="arrow2" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
    <path d="M2 1L8 5L2 9" fill="none" stroke="context-stroke" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  </marker>
</defs>
<text font-family="sans-serif" font-size="12" x="174" y="46" text-anchor="middle" fill="#5F5E5A">MCU #1</text>
<text font-family="sans-serif" font-size="12" x="514" y="46" text-anchor="middle" fill="#5F5E5A">MCU #2</text>
<line x1="344" y1="52" x2="344" y2="990" stroke="#B4B2A9" stroke-width="0.5" stroke-dasharray="6 3"/>
<rect x="30"  y="56" width="96" height="30" rx="6" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="78"  y="71" text-anchor="middle" dominant-baseline="central" fill="#3C3489">Sender A</text>
<rect x="138" y="56" width="96" height="30" rx="6" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="186" y="71" text-anchor="middle" dominant-baseline="central" fill="#3C3489">Sender B</text>
<rect x="246" y="56" width="96" height="30" rx="6" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="294" y="71" text-anchor="middle" dominant-baseline="central" fill="#3C3489">Receiver</text>
<rect x="354" y="56" width="96" height="30" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="402" y="71" text-anchor="middle" dominant-baseline="central" fill="#085041">Receiver</text>
<rect x="462" y="56" width="96" height="30" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="510" y="71" text-anchor="middle" dominant-baseline="central" fill="#085041">Logic</text>
<rect x="570" y="56" width="96" height="30" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="618" y="71" text-anchor="middle" dominant-baseline="central" fill="#085041">OLED</text>
<line x1="78"  y1="86" x2="78"  y2="990" stroke="#B4B2A9" stroke-width="0.5" stroke-dasharray="4 3"/>
<line x1="186" y1="86" x2="186" y2="990" stroke="#B4B2A9" stroke-width="0.5" stroke-dasharray="4 3"/>
<line x1="294" y1="86" x2="294" y2="990" stroke="#B4B2A9" stroke-width="0.5" stroke-dasharray="4 3"/>
<line x1="402" y1="86" x2="402" y2="990" stroke="#B4B2A9" stroke-width="0.5" stroke-dasharray="4 3"/>
<line x1="510" y1="86" x2="510" y2="990" stroke="#B4B2A9" stroke-width="0.5" stroke-dasharray="4 3"/>
<line x1="618" y1="86" x2="618" y2="990" stroke="#B4B2A9" stroke-width="0.5" stroke-dasharray="4 3"/>
<text font-family="sans-serif" font-size="12" x="78"  y="118" text-anchor="middle" fill="#5F5E5A">wakes (2s timer)</text>
<rect x="54"  y="138" width="48" height="24" rx="4" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="78"  y="150" text-anchor="middle" dominant-baseline="central" fill="#2C2C2A">take mutex</text>
<text font-family="sans-serif" font-size="12" x="186" y="178" text-anchor="middle" fill="#5F5E5A">blocked — mutex busy</text>
<rect x="162" y="188" width="48" height="150" rx="4" fill="#F1EFE8" stroke="#D3D1C7" stroke-width="0.5" stroke-dasharray="3 2"/>
<text font-family="sans-serif" font-size="12" x="78"  y="202" text-anchor="middle" fill="#5F5E5A">→ master mode</text>
<line x1="78"  y1="224" x2="402" y2="224" stroke="#2C2C2A" stroke-width="1.5" marker-end="url(#arrow2)"/>
<text font-family="sans-serif" font-size="12" x="240" y="218" text-anchor="middle" fill="#5F5E5A">HEARTBEAT over I2C wire</text>
<text font-family="sans-serif" font-size="12" x="402" y="248" text-anchor="middle" fill="#5F5E5A">ISR fires</text>
<text font-family="sans-serif" font-size="12" x="402" y="268" text-anchor="middle" fill="#5F5E5A">fills _rxBuf</text>
<text font-family="sans-serif" font-size="12" x="402" y="288" text-anchor="middle" fill="#5F5E5A">xSemaphoreGiveFromISR</text>
<text font-family="sans-serif" font-size="12" x="78"  y="316" text-anchor="middle" fill="#5F5E5A">→ slave mode</text>
<rect x="54"  y="328" width="48" height="24" rx="4" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="78"  y="340" text-anchor="middle" dominant-baseline="central" fill="#2C2C2A">give mutex</text>
<text font-family="sans-serif" font-size="12" x="186" y="360" text-anchor="middle" fill="#5F5E5A">unblocks</text>
<rect x="162" y="376" width="48" height="24" rx="4" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="186" y="388" text-anchor="middle" dominant-baseline="central" fill="#2C2C2A">take mutex</text>
<text font-family="sans-serif" font-size="12" x="402" y="416" text-anchor="middle" fill="#5F5E5A">wakes on semaphore</text>
<line x1="402" y1="432" x2="510" y2="432" stroke="#5F5E5A" stroke-width="1" marker-end="url(#arrow2)"/>
<text font-family="sans-serif" font-size="12" x="456" y="426" text-anchor="middle" fill="#5F5E5A">xQueueSend</text>
<text font-family="sans-serif" font-size="12" x="186" y="458" text-anchor="middle" fill="#5F5E5A">→ master mode</text>
<line x1="186" y1="478" x2="402" y2="478" stroke="#2C2C2A" stroke-width="1.5" marker-end="url(#arrow2)"/>
<text font-family="sans-serif" font-size="12" x="294" y="472" text-anchor="middle" fill="#5F5E5A">HEARTBEAT over I2C wire</text>
<text font-family="sans-serif" font-size="12" x="510" y="500" text-anchor="middle" fill="#5F5E5A">xQueueReceive</text>
<text font-family="sans-serif" font-size="12" x="510" y="520" text-anchor="middle" fill="#5F5E5A">parse + validate</text>
<text font-family="sans-serif" font-size="12" x="402" y="506" text-anchor="middle" fill="#5F5E5A">ISR fires</text>
<text font-family="sans-serif" font-size="12" x="402" y="522" text-anchor="middle" fill="#5F5E5A">xSemaphoreGiveFromISR</text>
<rect x="486" y="538" width="48" height="24" rx="4" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="510" y="550" text-anchor="middle" dominant-baseline="central" fill="#2C2C2A">take mutex</text>
<text font-family="sans-serif" font-size="12" x="186" y="574" text-anchor="middle" fill="#5F5E5A">→ slave mode</text>
<rect x="162" y="584" width="48" height="24" rx="4" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="186" y="596" text-anchor="middle" dominant-baseline="central" fill="#2C2C2A">give mutex</text>
<text font-family="sans-serif" font-size="12" x="510" y="578" text-anchor="middle" fill="#5F5E5A">→ master mode</text>
<line x1="510" y1="600" x2="294" y2="600" stroke="#2C2C2A" stroke-width="1.5" marker-end="url(#arrow2)"/>
<text font-family="sans-serif" font-size="12" x="402" y="594" text-anchor="middle" fill="#5F5E5A">HEARTBEAT_ACK over I2C wire</text>
<text font-family="sans-serif" font-size="12" x="510" y="624" text-anchor="middle" fill="#5F5E5A">→ slave mode</text>
<rect x="486" y="632" width="48" height="24" rx="4" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="510" y="644" text-anchor="middle" dominant-baseline="central" fill="#2C2C2A">give mutex</text>
<text font-family="sans-serif" font-size="12" x="618" y="662" text-anchor="middle" fill="#5F5E5A">reads state</text>
<text font-family="sans-serif" font-size="12" x="618" y="678" text-anchor="middle" fill="#5F5E5A">updates display</text>
<text font-family="sans-serif" font-size="12" x="294" y="628" text-anchor="middle" fill="#5F5E5A">ISR fires</text>
<text font-family="sans-serif" font-size="12" x="294" y="646" text-anchor="middle" fill="#5F5E5A">xSemaphoreGiveFromISR</text>
<text font-family="sans-serif" font-size="12" x="294" y="664" text-anchor="middle" fill="#5F5E5A">wakes on semaphore</text>
<line x1="294" y1="678" x2="78" y2="678" stroke="#5F5E5A" stroke-width="1" marker-end="url(#arrow2)"/>
<text font-family="sans-serif" font-size="12" x="186" y="672" text-anchor="middle" fill="#5F5E5A">xQueueSend (ACK)</text>
<text font-family="sans-serif" font-size="12" x="78"  y="700" text-anchor="middle" fill="#5F5E5A">xQueueReceive</text>
<text font-family="sans-serif" font-size="12" x="78"  y="718" text-anchor="middle" fill="#5F5E5A">print to serial</text>
<text font-family="sans-serif" font-size="12" x="78"  y="736" text-anchor="middle" fill="#5F5E5A">update OLED state</text>
<text font-family="sans-serif" font-size="12" x="618" y="718" text-anchor="middle" fill="#5F5E5A">reads state</text>
<text font-family="sans-serif" font-size="12" x="618" y="734" text-anchor="middle" fill="#5F5E5A">updates display</text>
<line x1="40"  y1="756" x2="640" y2="756" stroke="#D3D1C7" stroke-width="0.5" stroke-dasharray="3 3"/>
<text font-family="sans-serif" font-size="12" x="78"  y="776" text-anchor="middle" fill="#5F5E5A">vTaskDelay(2s)</text>
<text font-family="sans-serif" font-size="12" x="78"  y="794" text-anchor="middle" fill="#5F5E5A">↻ repeat</text>
<text font-family="sans-serif" font-size="12" x="186" y="776" text-anchor="middle" fill="#5F5E5A">vTaskDelay(3s)</text>
<text font-family="sans-serif" font-size="12" x="186" y="794" text-anchor="middle" fill="#5F5E5A">↻ repeat</text>
<text font-family="sans-serif" font-size="12" x="510" y="670" text-anchor="middle" fill="#5F5E5A">vTaskDelay</text>
<text font-family="sans-serif" font-size="12" x="510" y="686" text-anchor="middle" fill="#5F5E5A">↻ repeat</text>
<line x1="40"  y1="820" x2="640" y2="820" stroke="#D3D1C7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="40"  y="844" fill="#5F5E5A">Legend:</text>
<line x1="110" y1="844" x2="160" y2="844" stroke="#2C2C2A" stroke-width="1.5" marker-end="url(#arrow2)"/>
<text font-family="sans-serif" font-size="12" x="168" y="844" fill="#5F5E5A">I2C transmission on shared wire</text>
<line x1="420" y1="844" x2="470" y2="844" stroke="#5F5E5A" stroke-width="1" marker-end="url(#arrow2)"/>
<text font-family="sans-serif" font-size="12" x="478" y="844" fill="#5F5E5A">internal queue handoff</text>
<rect x="110" y="862" width="50" height="20" rx="3" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="135" y="872" text-anchor="middle" dominant-baseline="central" fill="#2C2C2A">mutex op</text>
<rect x="420" y="862" width="50" height="20" rx="3" fill="#F1EFE8" stroke="#D3D1C7" stroke-width="0.5" stroke-dasharray="3 2"/>
<text font-family="sans-serif" font-size="12" x="478" y="872" fill="#5F5E5A">task blocked on mutex</text>
</svg>

---

## Diagram 3 — Pass / fail criteria

What constitutes a passing result for each assumption. All three must pass
before proceeding to Phase 3.

<svg width="100%" viewBox="0 0 680 280" xmlns="http://www.w3.org/2000/svg">
<rect x="40"  y="52" width="180" height="36" rx="8" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="14" font-weight="500" x="130" y="70" text-anchor="middle" dominant-baseline="central" fill="#3C3489">A1 — mode switching</text>
<rect x="40"  y="96" width="180" height="60" rx="6" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="130" y="114" text-anchor="middle" fill="#5F5E5A">No BusError::BUS_FAULT</text>
<text font-family="sans-serif" font-size="12" x="130" y="130" text-anchor="middle" fill="#5F5E5A">on any send() call.</text>
<text font-family="sans-serif" font-size="12" x="130" y="146" text-anchor="middle" fill="#5F5E5A">ACKs arrive on MCU #1.</text>
<rect x="40"  y="166" width="180" height="28" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="130" y="180" text-anchor="middle" dominant-baseline="central" fill="#085041">verified via: serial monitor</text>
<rect x="250" y="52" width="180" height="36" rx="8" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="14" font-weight="500" x="340" y="70" text-anchor="middle" dominant-baseline="central" fill="#3C3489">A2 — queue integrity</text>
<rect x="250" y="96" width="180" height="60" rx="6" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="340" y="114" text-anchor="middle" fill="#5F5E5A">Every HEARTBEAT sent</text>
<text font-family="sans-serif" font-size="12" x="340" y="130" text-anchor="middle" fill="#5F5E5A">by MCU #1 appears as</text>
<text font-family="sans-serif" font-size="12" x="340" y="146" text-anchor="middle" fill="#5F5E5A">received on MCU #2.</text>
<rect x="250" y="166" width="180" height="28" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="340" y="180" text-anchor="middle" dominant-baseline="central" fill="#085041">verified via: serial monitor</text>
<rect x="460" y="52" width="180" height="36" rx="8" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="14" font-weight="500" x="550" y="70" text-anchor="middle" dominant-baseline="central" fill="#3C3489">A3 — mutex protection</text>
<rect x="460" y="96" width="180" height="60" rx="6" fill="#F1EFE8" stroke="#B4B2A9" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="550" y="114" text-anchor="middle" fill="#5F5E5A">Sender A and Sender B</text>
<text font-family="sans-serif" font-size="12" x="550" y="130" text-anchor="middle" fill="#5F5E5A">never produce garbled</text>
<text font-family="sans-serif" font-size="12" x="550" y="146" text-anchor="middle" fill="#5F5E5A">JSON on MCU #2.</text>
<rect x="460" y="166" width="180" height="28" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="12" x="550" y="180" text-anchor="middle" dominant-baseline="central" fill="#085041">verified via: serial + PulseView</text>
<rect x="40"  y="218" width="600" height="36" rx="8" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.5"/>
<text font-family="sans-serif" font-size="14" font-weight="500" x="340" y="236" text-anchor="middle" dominant-baseline="central" fill="#085041">all three pass → proceed to SharedBus v2 + full 5-MCU architecture</text>
</svg>
