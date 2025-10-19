import { useEffect, useState } from "react";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { ExternalLink, Code, BookOpen } from "lucide-react";
import { supabase } from "@/integrations/supabase/client";
import { useNavigate } from "react-router-dom";

interface Project {
  id: string;
  title: string;
  slug?: string | null;
  description: string;
  tech_stack: string[];
  image_url: string | null;
  demo_url: string | null;
  code_url: string | null;
  featured: boolean;
  case_study_content?: string | null;
}

const Projects = () => {
  const [projects, setProjects] = useState<Project[]>([]);
  const [loading, setLoading] = useState(true);
  const navigate = useNavigate();

  useEffect(() => {
    const fetchProjects = async () => {
      const { data, error } = await supabase
        .from("projects")
        .select("*")
        .order("featured", { ascending: false })
        .order("created_at", { ascending: false });

      if (!error && data) {
        setProjects(data);
      }
      setLoading(false);
    };

    fetchProjects();
  }, []);

  // Fallback sample projects if none exist in database
  const sampleProjects = [
    {
      id: "1",
      title: "E-Commerce Mobile App",
      description: "Full-featured shopping app built with Flutter and Firebase. Includes real-time inventory, payment integration, and push notifications.",
      tech_stack: ["Flutter", "Firebase", "Stripe"],
      image_url: null,
      demo_url: null,
      code_url: null,
      featured: true,
    },
    {
      id: "2",
      title: "Task Management API",
      description: "RESTful API service built with Django and PostgreSQL. Features JWT authentication, role-based access, and real-time updates.",
      tech_stack: ["Django", "PostgreSQL", "Redis"],
      image_url: null,
      demo_url: null,
      code_url: null,
      featured: false,
    },
    {
      id: "3",
      title: "Real-time Chat System",
      description: "High-performance chat server built with Golang. Supports WebSocket connections, message encryption, and file sharing.",
      tech_stack: ["Golang", "WebSocket", "MongoDB"],
      image_url: null,
      demo_url: null,
      code_url: null,
      featured: false,
    },
  ];

  const displayProjects = projects.length > 0 ? projects : sampleProjects;

  if (loading) {
    return (
      <section className="py-20 px-4" id="projects">
        <div className="container mx-auto text-center">
          <p>Loading projects...</p>
        </div>
      </section>
    );
  }

  return (
    <section className="py-20 px-4" id="projects">
      <div className="container mx-auto">
        <h2 className="text-4xl md:text-5xl font-bold text-center mb-4">
          Featured <span className="text-gradient">Projects</span>
        </h2>
        <p className="text-center text-muted-foreground mb-12 max-w-2xl mx-auto">
          Showcasing my best work across mobile and web development
        </p>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
          {displayProjects.map((project, index) => (
            <Card
              key={project.id}
              className="glass overflow-hidden group cursor-pointer hover:scale-105 transition-all duration-300"
              style={{
                animationDelay: `${index * 100}ms`,
                animationFillMode: "both",
              }}
            >
              {project.image_url ? (
                <div className="aspect-video overflow-hidden">
                  <img
                    src={project.image_url}
                    alt={project.title}
                    className="w-full h-full object-cover group-hover:scale-110 transition-transform duration-300"
                  />
                </div>
              ) : (
                <div className="aspect-video bg-gradient-to-br from-primary/20 to-accent/20 relative overflow-hidden">
                  <div className="absolute inset-0 flex items-center justify-center text-6xl opacity-20">
                    📱
                  </div>
                </div>
              )}
              <div className="p-6">
                <h3 className="text-2xl font-semibold mb-3">{project.title}</h3>
                <p className="text-muted-foreground mb-4">
                  {project.description}
                </p>
                <div className="flex flex-wrap gap-2 mb-4">
                  {project.tech_stack.map((tech, idx) => (
                    <Badge key={idx} variant="secondary">
                      {tech}
                    </Badge>
                  ))}
                </div>
                <div className="flex gap-2 flex-wrap">
                  {project.slug && project.case_study_content && (
                    <Button
                      variant="default"
                      size="sm"
                      className="flex-1 min-w-[100px]"
                      onClick={() => navigate(`/projects/${project.slug}`)}
                    >
                      <BookOpen className="mr-2 h-4 w-4" />
                      Case Study
                    </Button>
                  )}
                  {project.code_url && (
                    <Button
                      variant="outline"
                      size="sm"
                      className="flex-1 min-w-[100px]"
                      asChild
                    >
                      <a href={project.code_url} target="_blank" rel="noopener noreferrer">
                        <Code className="mr-2 h-4 w-4" />
                        Code
                      </a>
                    </Button>
                  )}
                  {project.demo_url && (
                    <Button variant="hero" size="sm" className="flex-1 min-w-[100px]" asChild>
                      <a href={project.demo_url} target="_blank" rel="noopener noreferrer">
                        <ExternalLink className="mr-2 h-4 w-4" />
                        Demo
                      </a>
                    </Button>
                  )}
                </div>
              </div>
            </Card>
          ))}
        </div>
      </div>
    </section>
  );
};

export default Projects;
