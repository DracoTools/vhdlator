-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

package accum_pkg is
  generic (type elem_t;
           zero : elem_t;
           function "+" (l, r : elem_t) return elem_t is <>);
  subtype value_t is elem_t;
  constant start : elem_t := zero;
  function twice (v : elem_t) return elem_t;
end package;

package body accum_pkg is
  function twice (v : elem_t) return elem_t is
  begin
    return v + v;
  end function;
end package body;

-- ---------------------------------------------------------------------------

-- An entity whose port type is a generic: one entity per type the design
-- gives it, which is what VHDL elaboration says (6.5.5)
entity doubler is
  generic (type t;
           function "+" (a, b : t) return t is <>);
  port (d : in  t;
        q : out t);
end entity;

architecture rtl of doubler is
begin
  q <= d + d;
end architecture;

-- ---------------------------------------------------------------------------

-- And an entity given a package rather than a type. Checked against nvc
-- 1.22.1, which agrees; ghdl 7.0.0-dev will not analyze a generic package on
-- an entity.
entity viapkg is
  generic (package p is new work.accum_pkg generic map (<>));
  port (d : in  p.value_t;
        q : out p.value_t);
end entity;

architecture rtl of viapkg is
  use p.all;
begin
  q <= twice(d);
end architecture;

-- ---------------------------------------------------------------------------

entity t_vhdl_gentype is
end entity;

architecture rtl of t_vhdl_gentype is
  -- The component's ports go by names of their own, and the binding says
  -- which of the entity's each becomes (7.3.2.2)
  component wider is
    generic (type t;
             function "+" (a, b : t) return t is <>);
    port (cd : in  t;
          cq : out t);
  end component;
  for all : wider use entity work.doubler
    port map (d => cd, q => cq);

  package int_accum is new work.accum_pkg generic map (elem_t => integer, zero => 0);

  signal ri, qi : integer := 0;
  signal rr, qr : real := 0.0;
  signal rc, qc : integer := 0;
  signal rp, qp : integer := 0;
begin
  u_int : entity work.doubler generic map (t => integer) port map (d => ri, q => qi);
  u_real : entity work.doubler generic map (t => real) port map (d => rr, q => qr);
  u_comp : wider generic map (t => integer) port map (rc, qc);
  u_pkg : entity work.viapkg generic map (p => int_accum) port map (d => rp, q => qp);

  main : process is
  begin
    ri <= 21;
    rr <= 1.5;
    rc <= 5;
    rp <= 7;
    wait for 1 ns;
    assert qi = 42 report "integer instance" severity failure;
    assert qr = 3.0 report "real instance" severity failure;
    assert qc = 10 report "component instance" severity failure;
    assert qp = 14 report "package instance" severity failure;
    ri <= -3;
    wait for 1 ns;
    assert qi = -6 report "integer again" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
