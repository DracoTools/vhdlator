-- DESCRIPTION: Verilator: VHDL test: 'BASE is only the prefix of another attribute, so it is no subtype an alias denotes (16.2.2; GHDL gna bug23482)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_aliasbase_bad is
end entity;

architecture a of t_vhdl_aliasbase_bad is
  alias bit_base is bit'base;
begin
end architecture;
