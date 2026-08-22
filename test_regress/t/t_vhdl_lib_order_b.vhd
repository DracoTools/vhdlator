-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The package body, which must follow its package declaration.

package body helper is
  function twice(x : integer) return integer is
  begin
    return x * 2;
  end function;
end package body;
