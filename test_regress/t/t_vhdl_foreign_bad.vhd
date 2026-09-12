-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Every way of getting the FOREIGN attribute wrong that analysis can see.

package foreign_bad_pkg is

  -- Not a subprogram
  signal s : integer;
  attribute foreign of s : signal is "VHPIDIRECT c_s";

  -- Value is not a static string
  function f1 (v : integer) return integer;
  attribute foreign of f1 : function is 3;

  -- Names nothing declared here
  attribute foreign of nosuch : function is "VHPIDIRECT c_nosuch";

  -- Says nothing
  function f2 (v : integer) return integer;
  attribute foreign of f2 : function is "";

  -- Says a kind but no symbol
  function f3 (v : integer) return integer;
  attribute foreign of f3 : function is "VHPIDIRECT";

  -- Every subprogram binds to a symbol of its own, so 'all' cannot mean one
  function f6 (v : integer) return integer;
  attribute foreign of all : function is "VHPIDIRECT c_all";

  -- Overloaded, so the C name would not say which
  function f5 (v : integer) return integer;
  function f5 (v : real) return real;
  attribute foreign of f5 : function is "VHPIDIRECT c_f5";

end package;
