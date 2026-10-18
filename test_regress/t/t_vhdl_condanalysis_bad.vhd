-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Conditional analysis that is in error (VHDL-2019 24.2): an identifier
-- nothing defines, directives with no "`if", a "`error" in the text being
-- analyzed and one in text that is not, conditions written wrong, and an
-- "`if" with no "`end"

entity t_vhdl_condanalysis_bad is
end entity;

architecture a of t_vhdl_condanalysis_bad is
`if NOT_DEFINED = "x" then
`end
`else
`elsif TOOL_TYPE = "x" then
`end
`if TOOL_TYPE = "SIMULATION" then
`error "reached"
`else
`error "never"
`end if
`warning "a warning"
`if VHDL_VERSION = "2019"
`end
`if (TOOL_TYPE = "x" then
`end
`if = "x" then
`end
`if TOOL_TYPE "x" then
`end
`if TOOL_TYPE = x then
`end
`if TOOL_TYPE = "x then
`end
`if TOOL_TYPE = "x" then junk
`end
begin
end architecture;
`if TOOL_TYPE = "SIMULATION" then
