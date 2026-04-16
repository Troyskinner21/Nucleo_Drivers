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
