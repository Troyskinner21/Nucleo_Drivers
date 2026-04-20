1. Why did you use snprintf instead of sprintf when transmitting a number?
Tests: buffer safety, overflow awareness, defensive programming

2. Your ReceiveUntil function has a timeout per byte — what happens if the sender is slow and you timeout mid-message?
Tests: understanding of blocking behavior, timeout granularity, partial receive problem

3. What's the problem with blocking UART receive in a real system and how would you solve it?
Tests: knowing when blocking is inappropriate, leads into interrupt and DMA discussion

4. Why do you NULL check the buffer pointer — can't the caller just make sure they pass a valid pointer?
Tests: defensive driver design, understanding that drivers are used by others

5. Your ReceiveUntil null terminates the buffer — why and what assumption does that make about the buffer size?
Tests: string handling, off by one awareness, the maxLen - 1 decision

6. What happens if the sender never sends the terminator character in ReceiveUntil?
Tests: understanding of timeout behavior, error path handling

7. Why does TransmitString cast const char* to uint8_t* — is that safe?
Tests: C type system understanding, const correctness, HAL API design

8. Your status enum has three values — OK, ERROR, TIMEOUT. How would you distinguish between a hardware error and a timeout in the caller?
Tests: error handling design, whether the driver gives enough information to the caller

9. If two tasks in an RTOS both call UART_Blocking_TransmitString at the same time what happens?
Tests: thread safety, re-entrancy, leads into mutex discussion

10. Why is blocking UART receive particularly dangerous in an interrupt-driven system?
Tests: understanding of execution context, what blocking means in different contexts

11. What does the timeout parameter actually represent in HAL_UART_Receive — is it total time or per byte time?
Tests: deep HAL knowledge, understanding what happens under the hood

12. How would you modify this driver to support multiple UART instances without duplicating code?
Tests: driver architecture, passing handles vs global state, scalability


### Must Know Cold
These three because they lead the whole conversation:

Question 3 — limitations of blocking and how to solve it. This is the most important one because it bridges to interrupt and DMA
Question 9 — RTOS thread safety. Direct connection to your FreeRTOS layer
Question 11 — what timeout actually means under the hood. Shows you understand what HAL is doing not just how to call it

### Should Know Well

Question 2 — timeout mid message problem. Very practical, comes up in real debugging
Question 6 — terminator never arrives. Same category, real world edge case
Question 12 — multiple UART instances. Shows architectural thinking

### Understand But Don't Need to Recite

Questions 1, 4, 5, 7, 8, 10 — these are good defensive programming habits you should practice in your code naturally. If you write the driver correctly they answer themselves. Nobody is going to ask you why you used snprintf in an embedded Linux interview.


# Interrupt questions
1. Why is txComplete declared as volatile and what happens if you forget it?
Tests: compiler optimization awareness, ISR and main context interaction, one of the most common embedded C interview questions

2. HAL_UART_Receive_IT only fires once — why and what happens if you forget to re-arm it in the callback?
Tests: understanding of how HAL interrupt mode actually works under the hood, a very common real world bug

3. Why do you check huart->Instance == USART2 inside the callback?
Tests: understanding that HAL callbacks are shared across all UART instances, what happens if you have USART1 and USART2 both active

4. There's a race condition possible between the ring buffer write in the ISR and the ring buffer read in main — where is it and how do you fix it?
Tests: critical section awareness, __disable_irq() / __enable_irq(), this is the most important question for this driver

5. What happens if bytes arrive faster than your main loop reads them from the ring buffer?
Tests: buffer overrun handling, dropped bytes vs blocking vs overflow flag, real world flow control

6. Why do you reset txComplete = 1 when HAL_UART_Transmit_IT fails to start?
Tests: error recovery, what happens if you don't reset it — system gets permanently stuck unable to TX

7. Your echo loop transmits one byte at a time — what's the performance problem with this and how would you fix it?
Tests: understanding of interrupt overhead per byte, batching transmissions, leading into DMA discussion

8. What's the difference between HAL_UART_Transmit_IT and HAL_UART_Transmit in terms of what the CPU is doing?
Tests: interrupt vs blocking at the hardware level, what actually happens inside each HAL function

9. Why can't you call HAL_UART_Transmit_IT again before txComplete is set — what actually happens inside HAL if you do?
Tests: HAL state machine understanding, HAL_BUSY return, gState internal to the handle

10. Your ring buffer is 128 bytes — how do you size it correctly for a real application?
Tests: understanding baud rate, processing latency, worst case burst size, real engineering tradeoff

11. Can you call HAL_UART_Transmit_IT from inside the HAL_UART_RxCpltCallback? What are the risks?
Tests: ISR execution context, calling HAL functions from ISR, latency impact on other interrupts

12. How does this driver break down if you add a second peripheral with higher priority interrupts?
Tests: NVIC priority understanding, interrupt preemption, priority inversion at the hardware level

13. The HAL_GetTick() timestamp pattern replaces HAL_Delay — what are the limitations of this approach as your main loop grows?
Tests: cooperative scheduling limitations, leads directly into FreeRTOS discussion, why RTOS tasks are cleaner

14. What happens to your echo loop if TX takes longer than the time between received bytes?
Tests: TX/RX rate mismatch, ring buffer filling up, back pressure handling

The Ones to Know Cold
Questions 4, 1, and 2 are the most important — in that order:

4 — the race condition question is the deepest one and shows real understanding of concurrent access
1 — volatile comes up in almost every embedded C interview
2 — re-arming is a classic gotcha that separates people who have actually used interrupt UART from people who just read about it

Notice how questions 7 and 13 both naturally lead the conversation toward DMA and FreeRTOS respectively — that's intentional. A good interviewer will use those as pivot points to go deeper into your knowledge of the next layers up the stack.