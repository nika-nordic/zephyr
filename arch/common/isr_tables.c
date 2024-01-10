/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/toolchain.h>
#include <zephyr/linker/sections.h>
#include <zephyr/sw_isr_table.h>
#include <zephyr/arch/cpu.h>

/* There is an additional member at the end populated by the linker script
 * which indicates the number of interrupts specified
 */
struct int_list_header {
	uint32_t table_size;
	uint32_t offset;
};

/* These values are not included in the resulting binary, but instead form the
 * header of the initList section, which is used by gen_isr_tables.py to create
 * the vector and sw isr tables,
 */
Z_GENERIC_SECTION(.irq_info) __used struct int_list_header _iheader = {
	.table_size = IRQ_TABLE_SIZE,
	.offset = CONFIG_GEN_IRQ_START_VECTOR,
};

/* These are placeholder tables. They will be replaced by the real tables
 * generated by gen_isr_tables.py.
 *
 * z_irq_spurious is used as a placeholder value to ensure that it is not
 * optimized out in the first linker pass. The first linker pass must contain
 * the same symbols as the second linker pass for the code generation to work.
 */

/* Some arches don't use a vector table, they have a common exception entry
 * point for all interrupts. Don't generate a table in this case.
 */
#ifdef CONFIG_GEN_IRQ_VECTOR_TABLE
/* When both the IRQ vector table and the software ISR table are used, populate
 * the IRQ vector table with the common software ISR by default, such that all
 * indirect interrupt vectors are handled using the software ISR table;
 * otherwise, populate the IRQ vector table with z_irq_spurious so that all
 * un-connected IRQ vectors end up in the spurious IRQ handler.
 */
#ifdef CONFIG_GEN_SW_ISR_TABLE
#define IRQ_VECTOR_TABLE_DEFAULT_ISR	_isr_wrapper
#else
#define IRQ_VECTOR_TABLE_DEFAULT_ISR	z_irq_spurious
#endif /* CONFIG_GEN_SW_ISR_TABLE */

#ifdef CONFIG_IRQ_VECTOR_TABLE_JUMP_BY_CODE

/* Assembly code for a jump instruction. Must be set by the architecture. */
#ifndef ARCH_IRQ_VECTOR_JUMP_CODE
#error "ARCH_IRQ_VECTOR_JUMP_CODE not defined"
#endif

#define BUILD_VECTOR(n, _) __asm(ARCH_IRQ_VECTOR_JUMP_CODE(IRQ_VECTOR_TABLE_DEFAULT_ISR))

/* The IRQ vector table contains the jump opcodes towards the vector routine */
void __irq_vector_table __attribute__((naked)) _irq_vector_table(void) {
	 LISTIFY(CONFIG_NUM_IRQS, BUILD_VECTOR, (;));
};
#else

/* The IRQ vector table is an array of vector addresses */
uintptr_t __irq_vector_table _irq_vector_table[IRQ_TABLE_SIZE] = {
	[0 ...(IRQ_TABLE_SIZE - 1)] = (uintptr_t)&IRQ_VECTOR_TABLE_DEFAULT_ISR,
};
#endif /* CONFIG_IRQ_VECTOR_TABLE_JUMP_BY_CODE */
#endif /* CONFIG_GEN_IRQ_VECTOR_TABLE */

/* If there are no interrupts at all, or all interrupts are of the 'direct'
 * type and bypass the _sw_isr_table, then do not generate one.
 */
#ifdef CONFIG_GEN_SW_ISR_TABLE
struct _isr_table_entry __sw_isr_table _sw_isr_table[IRQ_TABLE_SIZE] = {
	[0 ...(IRQ_TABLE_SIZE - 1)] = {(const void *)0x42,
				       (void *)&z_irq_spurious},
};
#endif

#ifdef CONFIG_SHARED_INTERRUPTS
struct z_shared_isr_table_entry __shared_sw_isr_table z_shared_sw_isr_table[IRQ_TABLE_SIZE] = {
};
#endif
