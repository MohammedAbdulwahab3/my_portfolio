-- Temporary fix for admin setup
-- This allows users to insert their own admin role if no admin exists yet

-- Create a function to check if any admin exists
CREATE OR REPLACE FUNCTION public.has_any_admin()
RETURNS BOOLEAN
LANGUAGE SQL
STABLE
AS $$
  SELECT EXISTS (
    SELECT 1 FROM public.user_roles 
    WHERE role = 'admin'::app_role
  );
$$;

-- Allow users to insert admin role for themselves if no admin exists
CREATE POLICY "Allow first admin setup"
ON public.user_roles
FOR INSERT
WITH CHECK (
  user_id = auth.uid() 
  AND role = 'admin'::app_role 
  AND NOT public.has_any_admin()
);

-- Also allow existing admins to manage roles
CREATE POLICY "Admins can manage all roles"
ON public.user_roles
FOR ALL
USING (public.has_role(auth.uid(), 'admin'::app_role));

-- Create a function to automatically make the first user an admin
CREATE OR REPLACE FUNCTION public.auto_assign_first_admin()
RETURNS TRIGGER
LANGUAGE plpgsql
SECURITY DEFINER
AS $$
BEGIN
  -- If this is the first user and no admin exists, make them admin
  IF NOT public.has_any_admin() THEN
    INSERT INTO public.user_roles (user_id, role)
    VALUES (NEW.id, 'admin'::app_role)
    ON CONFLICT DO NOTHING;
  END IF;
  
  RETURN NEW;
END;
$$;

-- Create trigger to auto-assign admin to first user
DROP TRIGGER IF EXISTS auto_assign_first_admin_trigger ON auth.users;
CREATE TRIGGER auto_assign_first_admin_trigger
  AFTER INSERT ON auth.users
  FOR EACH ROW
  EXECUTE FUNCTION public.auto_assign_first_admin();
