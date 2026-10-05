-- DESCRIPTION: Verilator: VHDL protected type body that names what the
-- package body's context clause makes visible
--
-- "use work.t_vhdl_protbodyuse_types.all;" written before "package body"
-- makes rec_t visible throughout the body, and a protected type body inside
-- it is a region nested in that one, so its variable may be of type rec_t
-- (12.1, 12.3; nvc sem issue276).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_protbodyuse_types is
  type rec_t is record
    field : integer;
  end record;
end package;

package t_vhdl_protbodyuse_pkg is
  type prot_t is protected
    procedure set(v : integer);
    impure function get return integer;
  end protected;
end package;

use work.t_vhdl_protbodyuse_types.all;

package body t_vhdl_protbodyuse_pkg is
  type prot_t is protected body
    variable rec : rec_t := (field => 3);
    procedure set(v : integer) is
    begin
      rec.field := v;
    end procedure;
    impure function get return integer is
    begin
      return rec.field;
    end function;
  end protected body;
end package body;

use work.t_vhdl_protbodyuse_pkg.all;

entity t_vhdl_protbodyuse is
end entity;

architecture a of t_vhdl_protbodyuse is
  shared variable p : prot_t;
begin
  process is
  begin
    assert p.get = 3 severity failure;
    p.set(11);
    assert p.get = 11 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
