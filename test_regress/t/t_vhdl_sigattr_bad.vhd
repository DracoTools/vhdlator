-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Of 'STABLE, 'QUIET, 'DELAYED and 'TRANSACTION an implicit signal is made,
-- one per prefix, to hold what the attribute answers about. A subprogram is
-- not a region a simulation cycle updates, so there is nowhere in one for
-- such a signal to live and reading one of a signal parameter is an error.
-- 'EVENT and 'LAST_VALUE make no such signal and stay legal.

entity t is
end entity;

architecture rtl of t is
  signal s : bit := '0';

  function legal (signal p : in bit) return boolean is
  begin
    return p'event and p'last_value = '0';
  end function;

  function bad_stable (signal p : in bit) return boolean is
  begin
    return p'stable;
  end function;

  function bad_quiet (signal p : in bit) return boolean is
  begin
    return p'quiet;
  end function;

  function bad_delayed (signal p : in bit) return bit is
  begin
    return p'delayed;
  end function;

begin
  -- Outside a subprogram the same attributes are ordinary
  process (s)
    variable v : boolean;
  begin
    v := s'stable and s'quiet;
    v := legal(s);
  end process;
end architecture;
