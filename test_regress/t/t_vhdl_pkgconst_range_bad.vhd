-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A package constant whose value is known only once the package body is
-- elaborated -- a function declared before it, its body after -- and is
-- outside its subtype (5.2.1, 14.4.2): the model stops when it starts, where
-- GHDL stops elaborating (gna bug081). nvc and Questa refuse the call before
-- its body at analysis, which is the VHDLELABORDER warning here.

package t_vhdl_pkgconst_range_bad_pkg is
  constant fish : integer := 4;
  function sticks return natural;
  constant not_negative : natural := fish - sticks;
end package;

package body t_vhdl_pkgconst_range_bad_pkg is
  function sticks return natural is
  begin
    return 5;
  end function;
end package body;

use work.t_vhdl_pkgconst_range_bad_pkg.all;

entity t_vhdl_pkgconst_range_bad is
end entity;

architecture behav of t_vhdl_pkgconst_range_bad is
begin
  process begin
    report "not stopped " & integer'image(not_negative) severity failure;
    wait;
  end process;
end architecture;
